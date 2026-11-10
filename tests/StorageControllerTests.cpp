/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include <gtest/gtest.h>
#include "emubase/Emubase.h"
#include <algorithm>
#include <array>
#include <vector>

void DebugLog(LPCTSTR) {}
const TCHAR* REGISTER_NAME[] = {L"R0",L"R1",L"R2",L"R3",L"R4",L"R5",L"SP",L"PC"};
#ifdef _DEBUG
int AssertFailedLine(const char*,int) { std::abort(); }
#endif
namespace {
class Image
{
    std::array<wchar_t,MAX_PATH> path{};
public:
    Image() { std::array<wchar_t,MAX_PATH> dir{}; EXPECT_NE(0,GetTempPathW(MAX_PATH,dir.data())); EXPECT_NE(0,GetTempFileNameW(dir.data(),L"ust",0,path.data())); }
    ~Image() { DeleteFileW(path.data()); }
    const wchar_t* Path() const { return path.data(); }
    void Write(const std::vector<uint8_t>& bytes) {
        UniqueFile file(_wfopen(Path(),L"wb")); ASSERT_NE(nullptr,file); ASSERT_EQ(bytes.size(),fwrite(bytes.data(),1,bytes.size(),file.get()));
    }
    std::vector<uint8_t> Read() const {
        UniqueFile file(_wfopen(Path(),L"rb")); if (!file) return {};
        fseek(file.get(),0,SEEK_END); const auto size=ftell(file.get()); rewind(file.get());
        if(size<0) return {};
        std::vector<uint8_t> bytes(size); EXPECT_EQ(bytes.size(),fread(bytes.data(),1,bytes.size(),file.get())); return bytes;
    }
};
constexpr uint16_t Data=0x1f0, Error=0x1f1, Count=0x1f2, Sector=0x1f3, Cylinder=0x1f4, Head=0x1f6, Status=0x1f7;
class HardProbe : public CHardDrive {
public:
    int Pending() const { return m_timeoutcount; }
    bool Attached() const { return m_fpFile!=nullptr; }
};
class FloppyProbe : public CFloppyController {
public:
    auto& Raw() { return m_pDrive->data; }
    auto& Markers() { return m_pDrive->marker; }
    void Dirty() { m_trackchanged=true; }
    bool Changed() const { return m_trackchanged; }
    auto Position() const { return std::array<uint16_t,3>{m_drive,m_track,m_side}; }
    void Reopen(const wchar_t* path,const wchar_t* mode) { m_pDrive->fpFile.reset(); m_pDrive->fpFile.reset(_wfopen(path,mode)); ASSERT_NE(nullptr,m_pDrive->fpFile); }
    bool Queued() const { return m_writeflag || m_shiftflag; }
};
constexpr uint16_t Select0=02003|FLOPPY_CMD_ENGINESTART;
}
class HardSequences : public testing::Test {
protected:
    Image file;
    HardProbe hd;
    std::vector<uint8_t> original;
    void SetUp() override {
        original.resize(512*320);
        for(size_t i=0;i<original.size();++i) original[i]=static_cast<uint8_t>((i/512)*7+i*3);
        original[0]=8; original[1]=2; std::fill_n(original.begin()+0x1f0,12,0);
        file.Write(original); ASSERT_TRUE(hd.AttachImage(file.Path())); hd.Reset(); Tick(2);
    }
    void Tick(int n) { for(int i=0;i<n;++i) hd.Periodic(); }
    uint16_t R(uint16_t port) { return hd.ReadPort(port)&255; }
    void Setup(int c,int h,int s,int n) {
        hd.WritePort(Cylinder,c); hd.WritePort(Cylinder+1,c>>8); hd.WritePort(Head,h); hd.WritePort(Sector,s); hd.WritePort(Count,n);
    }
    void ReadBlock(int lba) {
        ASSERT_EQ(8,R(Status)&0x89);
        for(int i=0;i<256;++i) {
            const size_t offset=lba*512+i*2;
            EXPECT_EQ(static_cast<uint16_t>(~(original[offset]|(original[offset+1]<<8))),hd.ReadPort(Data));
        }
    }
    void WriteBlock(uint16_t value) { ASSERT_EQ(8,R(Status)&0x89); for(int i=0;i<256;++i) hd.WritePort(Data,value); }
};
TEST_F(HardSequences, ReadDelayDataBoundaryAndAlias) {
    Setup(0,0,2,1); hd.WritePort(Status,0x21);
    EXPECT_EQ(0x80,R(Status)&0x88); EXPECT_EQ(0,hd.ReadPort(Data));
    Tick(767); EXPECT_NE(0,R(Status)&0x80); Tick(1); EXPECT_EQ(0,R(Count));
    ReadBlock(1); EXPECT_EQ(0,R(Status)&0x89); EXPECT_EQ(0,hd.ReadPort(Data));
}
TEST_F(HardSequences, ZeroCountReadsExactly256Sectors) {
    Setup(0,0,1,0); hd.WritePort(Status,0x20); Tick(768);
    for(int sector=0;sector<256;++sector) {
        ASSERT_EQ(255-sector,R(Count)); ReadBlock(sector);
        if(sector<255) { EXPECT_EQ(0x80,R(Status)&0x88); Tick(512); }
    }
    EXPECT_EQ(0,R(Status)&0x89); EXPECT_EQ(0,hd.Pending());
}
TEST_F(HardSequences, ZeroCountWrites256SectorsWithChsRollover) {
    Setup(0,0,1,0); hd.WritePort(Status,0x31);
    for(int sector=0;sector<256;++sector) {
        WriteBlock(static_cast<uint16_t>(sector)); EXPECT_EQ(0x80,R(Status)&0x88);
        Tick(255); EXPECT_NE(0,R(Status)&0x80); Tick(1); EXPECT_EQ(255-sector,R(Count));
    }
    EXPECT_EQ(0,R(Status)&0x89); hd.DetachImage(); const auto result=file.Read(); ASSERT_EQ(original.size(),result.size());
    for(int sector=0;sector<256;++sector) for(int word=0;word<256;++word) {
        EXPECT_EQ(static_cast<uint8_t>(~sector),result[sector*512+word*2]); EXPECT_EQ(255,result[sector*512+word*2+1]);
    }
    EXPECT_TRUE(std::equal(result.begin()+256*512,result.end(),original.begin()+256*512));
}
TEST_F(HardSequences, IdentifyDrainsWithoutSchedulingDiskRead) {
    Setup(0,0,1,7); hd.WritePort(Status,0xec);
    for(int i=0;i<256;++i) hd.ReadPort(Data);
    EXPECT_EQ(0,R(Status)&0x89); EXPECT_EQ(0,R(Count)); EXPECT_EQ(0,hd.Pending());
    Tick(1000); EXPECT_EQ(0,R(Status)&0x89);
}
TEST_F(HardSequences, IdentifyReplacesPendingRead) {
    Setup(0,0,2,1); hd.WritePort(Status,0x20); Tick(5); hd.WritePort(Status,0xec);
    Tick(1000); EXPECT_EQ(static_cast<uint16_t>(~0x045a),hd.ReadPort(Data));
    EXPECT_EQ(0,hd.Pending());
}
TEST_F(HardSequences, ResetCancelsPartialAndScheduledWrites) {
    for(int words : {17,256}) {
        Setup(0,0,2,1); hd.WritePort(Status,0x30);
        for(int i=0;i<words;++i) hd.WritePort(Data,0x1234);
        hd.Reset(); EXPECT_EQ(0x80,R(Status)); Tick(1); EXPECT_EQ(0x80,R(Status)); Tick(1000);
        EXPECT_EQ(0x50,R(Status)); EXPECT_EQ(0,hd.ReadPort(Data));
    }
    hd.DetachImage(); EXPECT_EQ(original,file.Read());
}
TEST_F(HardSequences, DetachCancelsPendingTransferBeforeReattach) {
    Setup(0,0,2,1); hd.WritePort(Status,0x20); hd.DetachImage();
    ASSERT_EQ(0,hd.Pending()); EXPECT_EQ(0,R(Status)&8); Tick(1000);
    ASSERT_TRUE(hd.AttachImage(file.Path())); hd.Reset(); Tick(2); Setup(0,0,2,1); hd.WritePort(Status,0x20); Tick(768); ReadBlock(1);
}
TEST_F(HardSequences, DetachCancelsScheduledWrite) {
    Setup(0,0,2,1); hd.WritePort(Status,0x30); WriteBlock(0x1234); hd.DetachImage();
    ASSERT_EQ(0,hd.Pending()); Tick(1000); EXPECT_EQ(original,file.Read());
}
TEST_F(HardSequences, DataWritesWithoutRequestDoNotChangeImage) {
    for(int i=0;i<512;++i) hd.WritePort(Data,0x1234);
    Tick(1000); hd.DetachImage(); EXPECT_EQ(original,file.Read());
}
TEST_F(HardSequences, WriteReplacesPendingReadWithoutOverwritingItsBuffer) {
    Setup(0,0,2,1); hd.WritePort(Status,0x20); Tick(100); hd.WritePort(Status,0x30);
    hd.WritePort(Data,0x1234); Tick(1000); ASSERT_EQ(8,R(Status)&0x89);
    for(int i=1;i<256;++i) hd.WritePort(Data,0x5678);
    Tick(256); EXPECT_EQ(0,R(Status)&0x89); hd.DetachImage();
    auto expected=original; expected[512]=0xcb; expected[513]=0xed;
    for(int i=1;i<256;++i) { expected[512+i*2]=0x87; expected[513+i*2]=0xa9; }
    EXPECT_EQ(expected,file.Read());
}
TEST_F(HardSequences, DetachedReadAndWriteFailWithoutAccessingClosedStream) {
    hd.DetachImage();
    for(int command : {0x20,0x30}) {
        Setup(0,0,2,1); hd.WritePort(Status,command);
        if(command==0x30) WriteBlock(0x1234);
        Tick(768); EXPECT_EQ(1,R(Status)&0x89); EXPECT_EQ(0x80,R(Error));
    }
    EXPECT_EQ(original,file.Read());
}
TEST_F(HardSequences, ReadFailureClearsRequestAndRecoversOnNextRead) {
    Setup(20,0,1,1); hd.WritePort(Status,0x20); Tick(768);
    EXPECT_EQ(1,R(Status)&0x89); EXPECT_EQ(0x80,R(Error)); EXPECT_EQ(1,R(Count)); EXPECT_EQ(0,hd.ReadPort(Data));
    Setup(0,0,2,1); hd.WritePort(Status,0x20); Tick(768); ReadBlock(1); EXPECT_EQ(0,R(Error));
}
TEST_F(HardSequences, EndOfImageStopsMultiSectorTransferWithoutWrapping) {
    Setup(19,1,8,2); hd.WritePort(Status,0x20); Tick(768); ReadBlock(319); Tick(512);
    EXPECT_EQ(1,R(Status)&0x89); EXPECT_EQ(1,R(Count)); EXPECT_EQ(20,R(Cylinder)); EXPECT_EQ(1,R(Sector));
}
class InvalidChs : public HardSequences,public testing::WithParamInterface<std::array<int,3>> {};
TEST_P(InvalidChs, ReadAndWriteRejectInvalidLocationWithoutChangingImage) {
    const auto chs=GetParam();
    for(int command : {0x20,0x30}) {
        Setup(chs[0],chs[1],chs[2],1); hd.WritePort(Status,command);
        if(command==0x30) for(int i=0;i<256;++i) hd.WritePort(Data,0x1234);
        Tick(768); EXPECT_EQ(1,R(Status)&0x89); EXPECT_EQ(0x80,R(Error)); EXPECT_EQ(1,R(Count));
    }
    hd.DetachImage(); EXPECT_EQ(original,file.Read());
}
INSTANTIATE_TEST_SUITE_P(Boundaries,InvalidChs,testing::Values(std::array{0,0,0},std::array{0,0,9},std::array{0,2,1},std::array{20,0,1},std::array{65535,15,255}));
class FloppySequences : public testing::Test {
protected:
    Image file;
    FloppyProbe f;
    std::vector<uint8_t> original=std::vector<uint8_t>(10240,0x35);
    void SetUp() override { file.Write(original); ASSERT_TRUE(f.AttachImage(0,file.Path())); f.SetCommand(Select0); }
    void AtData() { for(int i=0;i<47;++i) f.Periodic(); }
};
TEST_F(FloppySequences, WritePipelinePreservesFirstAndReplacesQueuedWord) {
    AtData(); f.WriteData(0x1234); EXPECT_NE(0,f.GetState()&FLOPPY_STATUS_MOREDATA);
    f.WriteData(0x5678); EXPECT_EQ(0,f.GetState()&FLOPPY_STATUS_MOREDATA); f.WriteData(0xabcd);
    f.Periodic(); EXPECT_NE(0,f.GetState()&FLOPPY_STATUS_MOREDATA); f.Periodic();
    EXPECT_EQ(0x34,f.Raw()[96]); EXPECT_EQ(0x12,f.Raw()[97]); EXPECT_EQ(0xcd,f.Raw()[98]); EXPECT_EQ(0xab,f.Raw()[99]);
    EXPECT_FALSE(f.Queued()); f.DetachImage(0); auto expected=original; expected[0]=0x34; expected[1]=0x12; expected[2]=0xcd; expected[3]=0xab; EXPECT_EQ(expected,file.Read());
}
TEST_F(FloppySequences, ReadingCancelsQueuedWrites) {
    AtData(); f.WriteData(0x1234); f.WriteData(0x5678); f.GetData(); f.Periodic();
    EXPECT_FALSE(f.Queued()); EXPECT_FALSE(f.Changed()); EXPECT_EQ(0x35,f.Raw()[96]);
}
TEST_F(FloppySequences, ResetFlushesWrittenDataAndDiscardsQueuedWord) {
    AtData(); f.WriteData(0x1234); f.WriteData(0x5678); f.Periodic(); f.Reset();
    EXPECT_FALSE(f.Queued()); EXPECT_FALSE(f.Changed()); EXPECT_FALSE(f.IsEngineOn());
    auto expected=original; expected[0]=0x34; expected[1]=0x12; EXPECT_EQ(expected,file.Read());
}
TEST_F(FloppySequences, MarkerFollowsQueuedWordAndClearsOnLaterWrites) {
    AtData(); f.WriteData(0xa1a1); f.WriteData(0xfba1); f.SetCommand(Select0|FLOPPY_CMD_SKIPSYNC);
    f.Periodic(); f.WriteData(0x1234); f.Periodic(); f.Periodic();
    EXPECT_EQ(0,f.Markers()[48]); EXPECT_EQ(1,f.Markers()[49]); EXPECT_EQ(0,f.Markers()[50]);
}
class FailedFlush : public FloppySequences,public testing::WithParamInterface<int> {};
TEST_P(FailedFlush, RetainsTrackAndSelectionUntilStreamCanBeWritten) {
    f.Raw()[96]=0x7a; f.Dirty(); const auto raw=f.Raw(); const auto position=f.Position();
    // A valid read-only stream injects a real fwrite failure without closing a live FILE.
    f.Reopen(file.Path(),L"rb");
    switch(GetParam()) {
    case 0: f.Reset(); break;
    case 1: f.SetCommand(Select0|FLOPPY_CMD_SIDEUP); break;
    case 2: f.SetCommand(Select0|FLOPPY_CMD_STEP|FLOPPY_CMD_DIR); break;
    case 3: f.SetCommand(02002); break;
    case 4: f.DetachImage(0); break;
    case 5: EXPECT_FALSE(f.AttachImage(1,file.Path())); break;
    }
    EXPECT_EQ(position,f.Position()); EXPECT_EQ(raw,f.Raw()); EXPECT_TRUE(f.Changed()); EXPECT_TRUE(f.IsAttached(0));
    EXPECT_NE(0,f.GetState()&FLOPPY_STATUS_WRITEPROTECT); EXPECT_EQ(original,file.Read());
    f.Reopen(file.Path(),L"r+b"); f.Reset(); EXPECT_FALSE(f.Changed()); EXPECT_EQ(0,f.GetState()&FLOPPY_STATUS_WRITEPROTECT);
    auto expected=original; expected[0]=0x7a; EXPECT_EQ(expected,file.Read());
}
INSTANTIATE_TEST_SUITE_P(Operations,FailedFlush,testing::Range(0,6));
TEST_F(FloppySequences, MalformedTrackBlocksResetAndCanBeRepaired) {
    const auto originalRaw=f.Raw(); f.Raw().fill(0); f.Dirty(); f.Reset(); EXPECT_TRUE(f.Changed()); EXPECT_EQ(original,file.Read());
    f.Raw()=originalRaw; f.Reset(); EXPECT_FALSE(f.Changed()); EXPECT_EQ(original,file.Read());
}
