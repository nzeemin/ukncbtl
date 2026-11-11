/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Hard.cpp
/// \brief Hard disk drive emulation.
/// \details See defines in header file Emubase.h

#include "stdafx.h"
#include <algorithm>
#include <string_view>
#include <sys/stat.h>
#include "Emubase.h"
#include "util/LittleEndian.h"


//////////////////////////////////////////////////////////////////////
// Constants

static constexpr auto TIME_PER_SECTOR = IDE_DISK_SECTOR_SIZE / 2;

static constexpr auto IDE_PORT_DATA = 0x1f0;
static constexpr auto IDE_PORT_ERROR = 0x1f1;
static constexpr auto IDE_PORT_SECTOR_COUNT = 0x1f2;
static constexpr auto IDE_PORT_SECTOR_NUMBER = 0x1f3;
static constexpr auto IDE_PORT_CYLINDER_LSB = 0x1f4;
static constexpr auto IDE_PORT_CYLINDER_MSB = 0x1f5;
static constexpr auto IDE_PORT_HEAD_NUMBER = 0x1f6;
static constexpr auto IDE_PORT_STATUS_COMMAND = 0x1f7;

static constexpr auto IDE_STATUS_ERROR = 0x01;
static constexpr auto IDE_STATUS_HIT_INDEX = 0x02;
static constexpr auto IDE_STATUS_BUFFER_READY = 0x08;
static constexpr auto IDE_STATUS_SEEK_COMPLETE = 0x10;
static constexpr auto IDE_STATUS_DRIVE_READY = 0x40;
static constexpr auto IDE_STATUS_BUSY = 0x80;

static constexpr auto IDE_COMMAND_READ_MULTIPLE = 0x20;
static constexpr auto IDE_COMMAND_READ_MULTIPLE1 = 0x21;
static constexpr auto IDE_COMMAND_SET_CONFIG = 0x91;
static constexpr auto IDE_COMMAND_WRITE_MULTIPLE = 0x30;
static constexpr auto IDE_COMMAND_WRITE_MULTIPLE1 = 0x31;
static constexpr auto IDE_COMMAND_IDENTIFY = 0xec;

static constexpr auto IDE_ERROR_NONE = 0x00;
static constexpr auto IDE_ERROR_DEFAULT = 0x01;
static constexpr auto IDE_ERROR_UNKNOWN_COMMAND = 0x04;
static constexpr auto IDE_ERROR_BAD_LOCATION = 0x10;
static constexpr auto IDE_ERROR_BAD_SECTOR = 0x80;


//////////////////////////////////////////////////////////////////////

// Inverts 512 bytes in the buffer
static void InvertBuffer(std::span<uint8_t, IDE_DISK_SECTOR_SIZE> buffer)
{
    for (auto& byte : buffer)
        byte = static_cast<uint8_t>(~byte);
}

//////////////////////////////////////////////////////////////////////


CHardDrive::CHardDrive()
{
    m_fpFile = nullptr;

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;
    m_command = 0;
    m_timeoutcount = 0;
    m_sectorcount = 0;

    m_numsectors = m_numheads = m_numcylinders = 0;
    m_curcylinder = m_curhead = m_curheadreg = m_cursector = m_bufferoffset = 0;
    m_buffer.fill(0);

    m_okInverted = false;
    m_okReadOnly = false;
}

CHardDrive::~CHardDrive()
{
    DetachImage();
}

void CHardDrive::Reset()
{
    //DebugLog(_T("HDD Reset\r\n"));

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;
    m_command = 0;
    m_timeoutcount = 2;
    m_timeoutevent = TimeoutEvent::reset_done;
}

bool CHardDrive::AttachImage(LPCTSTR sFileName)
{
    ASSERT(sFileName != nullptr);
    if (sFileName == nullptr || m_fpFile != nullptr)
        return false;

    // Open file
    m_okReadOnly = false;
    UniqueFile file(::_tfopen(sFileName, _T("r+b")));
    if (!file)
    {
        m_okReadOnly = true;
        file.reset(::_tfopen(sFileName, _T("rb")));
    }
    if (!file)
        return false;

    // Check file size
    if (::fseek(file.get(), 0, SEEK_END) != 0)
        return false;
    const long fileSize = ::ftell(file.get());
    if (fileSize < 0 || ::fseek(file.get(), 0, SEEK_SET) != 0)
        return false;
    uint32_t dwFileSize = static_cast<uint32_t>(fileSize);
    if (dwFileSize % 512 != 0)
        return false;

    // Read first sector
    size_t dwBytesRead = ::fread(m_buffer.data(), 1, 512, file.get());
    if (dwBytesRead != 512)
        return false;

    // Detect hard disk type
    const auto sector = std::span{m_buffer};
    const auto signature = little_endian::read<uint16_t>(sector.subspan<0, 2>());
    const auto marker1 = little_endian::read<uint16_t>(sector.subspan<2, 2>());
    const auto marker2 = little_endian::read<uint16_t>(sector.subspan<4, 2>());
    const bool invertedHeader = signature == 0xAB56 && marker1 == 0x0010 && marker2 == 0x0100;
    if ((signature == 0x54A9 && marker1 == 0xFFEF && marker2 == 0xFEFF) || invertedHeader)
    {
        m_okInverted = invertedHeader;
        if (m_okInverted)
            InvertBuffer(m_buffer);

        // Calculate geometry
        m_numsectors = little_endian::read<uint16_t>(sector.subspan<8, 2>());
        if (m_numsectors == 0)
            return false;  // Geometry params are not defined
        const auto sectorsPerCylinder = little_endian::read<uint16_t>(sector.subspan<10, 2>());
        m_numheads = static_cast<uint8_t>(sectorsPerCylinder / m_numsectors);
        if (m_numheads == 0)
            return false;  // Geometry params are not defined
        m_numcylinders = dwFileSize / 512 / m_numsectors / m_numheads;
        if (m_numcylinders == 0)
            return false;
    }
    else  // ID or WD type
    {
        // Autodetect inverted image
        uint8_t test = 0xff;
        for (int i = 0x1f0; i <= 0x1fb; i++)
            test &= m_buffer[i];
        m_okInverted = (test == 0xff);
        // Invert the buffer if needed
        if (m_okInverted)
            InvertBuffer(m_buffer);

        // Calculate geometry
        m_numsectors = m_buffer[0];
        m_numheads = m_buffer[1];
        if (m_numsectors == 0 || m_numheads == 0)
            return false;  // Geometry params are not defined
        m_numcylinders = dwFileSize / 512 / m_numsectors / m_numheads;
        if (m_numcylinders == 0 || m_numcylinders > 1024)
            return false;
    }

    m_fpFile = std::move(file);
    m_curcylinder = m_curhead = m_curheadreg = m_cursector = m_bufferoffset = 0;

    m_status = IDE_STATUS_BUSY;
    m_error = IDE_ERROR_NONE;

    return true;
}

void CHardDrive::DetachImage()
{
    m_timeoutcount = 0;
    m_timeoutevent = TimeoutEvent::none;
    m_status = IDE_STATUS_BUSY;
    m_command = 0;
    m_sectorcount = m_bufferoffset = 0;
    m_fpFile.reset();
}

uint16_t CHardDrive::ReadPort(uint16_t port)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

    uint16_t data = 0;
    switch (port)
    {
    case IDE_PORT_DATA:
        if (m_status & IDE_STATUS_BUFFER_READY)
        {
            data = static_cast<uint16_t>(m_buffer[m_bufferoffset] |
                (m_buffer[m_bufferoffset + 1] << 8));
            if (!m_okInverted)
                data = ~data;  // Image stored non-inverted, but QBUS inverts the bits
            m_bufferoffset += 2;

            //if (m_bufferoffset == 2)
            //    DebugLogFormat(_T("HDD Read sector start {:04x}\r\n"), data);

            if (m_bufferoffset >= IDE_DISK_SECTOR_SIZE)
            {
                //DebugLog(_T("HDD Read sector complete\r\n"));

                ContinueRead();
            }
        }
        break;
    case IDE_PORT_ERROR:
        data = 0xff00 | m_error;
        break;
    case IDE_PORT_SECTOR_COUNT:
        data = 0xff00 | (uint16_t)m_sectorcount;
        break;
    case IDE_PORT_SECTOR_NUMBER:
        data = 0xff00 | (uint16_t)m_cursector;
        break;
    case IDE_PORT_CYLINDER_LSB:
        data = 0xff00 | (uint16_t)(m_curcylinder & 0xff);
        break;
    case IDE_PORT_CYLINDER_MSB:
        data = 0xff00 | (uint16_t)((m_curcylinder >> 8) & 0xff);
        break;
    case IDE_PORT_HEAD_NUMBER:
        data = 0xff00 | (uint16_t)m_curheadreg;
        break;
    case IDE_PORT_STATUS_COMMAND:
        data = 0xff00 | m_status;
        break;
    }

    //DebugPrintFormat(_T("HDD Read  {:x}     0x{:04x}\r\n"), port, data);
    return data;
}

void CHardDrive::WritePort(uint16_t port, uint16_t data)
{
    ASSERT(port >= 0x1F0 && port <= 0x1F7);

//    DebugPrintFormat(_T("HDD Write {:x} <-- 0x{:04x}\r\n"), port, data);

    switch (port)
    {
    case IDE_PORT_DATA:
        if (m_status & IDE_STATUS_BUFFER_READY)
        {
            if (!m_okInverted)
                data = ~data;  // Image stored non-inverted, but QBUS inverts the bits
            m_buffer[m_bufferoffset] = static_cast<uint8_t>(data);
            m_buffer[m_bufferoffset + 1] = static_cast<uint8_t>(data >> 8);
            m_bufferoffset += 2;

            //if (m_bufferoffset == 2)
            //    DebugLogFormat(_T("HDD Write sector start {:04x}\r\n"), data);

            if (m_bufferoffset >= IDE_DISK_SECTOR_SIZE)
            {
                m_status &= ~IDE_STATUS_BUFFER_READY;

                ContinueWrite();
            }
        }
        break;
    case IDE_PORT_ERROR:
        // Writing precompensation value -- ignore
        break;
    case IDE_PORT_SECTOR_COUNT:
        data &= 0x0ff;
        m_sectorcount = (data == 0) ? 256 : data;
        break;
    case IDE_PORT_SECTOR_NUMBER:
        data &= 0x0ff;
        m_cursector = data;
        break;
    case IDE_PORT_CYLINDER_LSB:
        data &= 0x0ff;
        m_curcylinder = (m_curcylinder & 0xff00) | (data & 0xff);
        break;
    case IDE_PORT_CYLINDER_MSB:
        data &= 0x0ff;
        m_curcylinder = (m_curcylinder & 0x00ff) | ((data & 0xff) << 8);
        break;
    case IDE_PORT_HEAD_NUMBER:
        data &= 0x0ff;
        m_curhead = data & 0x0f;
        m_curheadreg = data;
        break;
    case IDE_PORT_STATUS_COMMAND:
        data &= 0x0ff;
        HandleCommand((uint8_t)data);
        break;
    }
}

// Called from CMotherboard::SystemFrame() every tick
void CHardDrive::Periodic()
{
    if (m_timeoutcount > 0)
    {
        m_timeoutcount--;
        if (m_timeoutcount == 0)
        {
            const auto evt = std::exchange(m_timeoutevent, TimeoutEvent::none);
            switch (evt)
            {
            case TimeoutEvent::reset_done:
                m_status &= ~IDE_STATUS_BUSY;
                m_status |= IDE_STATUS_DRIVE_READY | IDE_STATUS_SEEK_COMPLETE;
                break;
            case TimeoutEvent::read_sector_done:
                ReadSectorDone();
                break;
            case TimeoutEvent::write_sector_done:
                WriteSectorDone();
                break;
            }
        }
    }
}

void CHardDrive::HandleCommand(uint8_t command)
{
    m_command = command;
    switch (command)
    {
    case IDE_COMMAND_READ_MULTIPLE:
    case IDE_COMMAND_READ_MULTIPLE1:
        //DebugLogFormat(_T("HDD COMMAND {:02x} (READ MULT): C={}, H={}, SN={}, SC={}\r\n"),
        //        command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);

        m_status |= IDE_STATUS_BUSY;
        m_status &= ~IDE_STATUS_BUFFER_READY;

        m_timeoutcount = TIME_PER_SECTOR * 3;  // Timeout while seek for track
        m_timeoutevent = TimeoutEvent::read_sector_done;
        break;

    case IDE_COMMAND_SET_CONFIG:
        //DebugLogFormat(_T("HDD COMMAND {:02x} (SET CONFIG): H={}, SC={}\r\n"),
        //        command, m_curhead, m_sectorcount);

        m_numsectors = m_sectorcount;
        m_numheads = m_curhead + 1;
        break;

    case IDE_COMMAND_WRITE_MULTIPLE:
    case IDE_COMMAND_WRITE_MULTIPLE1:
        //DebugLogFormat(_T("HDD COMMAND {:02x} (WRITE MULT): C={}, H={}, SN={}, SC={}\r\n"),
        //        command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);

        m_timeoutcount = 0;
        m_timeoutevent = TimeoutEvent::none;
        m_status &= ~(IDE_STATUS_BUSY | IDE_STATUS_ERROR);
        m_error = IDE_ERROR_NONE;
        m_bufferoffset = 0;
        m_status |= IDE_STATUS_BUFFER_READY;
        break;

    case IDE_COMMAND_IDENTIFY:
        //DebugLogFormat(_T("HDD COMMAND {:02x} (IDENTIFY)\r\n"), command);

        IdentifyDrive();  // Prepare the buffer
        m_timeoutcount = 0;
        m_timeoutevent = TimeoutEvent::none;
        m_bufferoffset = 0;
        m_sectorcount = 0;  // IDENTIFY has no following disk sectors to read.
        m_status |= IDE_STATUS_BUFFER_READY | IDE_STATUS_SEEK_COMPLETE | IDE_STATUS_DRIVE_READY;
        m_status &= ~IDE_STATUS_BUSY;
        m_status &= ~IDE_STATUS_ERROR;
        break;

    default:
        //DebugLogFormat(_T("HDD COMMAND {:02x} (UNKNOWN): C={}, H={}, SN={}, SC={}\r\n"),
        //        command, m_curcylinder, m_curhead, m_cursector, m_sectorcount);
        break;
    }
}

// Copy the string to the destination, swapping bytes in every word
void CHardDrive::IdentifyDrive()
{
    const uint32_t totalSectors = static_cast<uint32_t>(m_numcylinders) * m_numheads * m_numsectors;
    std::ranges::fill(m_buffer, 0);
    const auto writeWord = [this](size_t index, uint16_t value)
    {
        m_buffer[index * 2] = static_cast<uint8_t>(value);
        m_buffer[index * 2 + 1] = static_cast<uint8_t>(value >> 8);
    };
    const auto writeDword = [&writeWord](size_t index, uint32_t value)
    {
        writeWord(index, static_cast<uint16_t>(value));
        writeWord(index + 1, static_cast<uint16_t>(value >> 16));
    };
    const auto writeText = [this](size_t index, size_t words, std::string_view text)
    {
        for (size_t i = 0; i < words * 2; ++i)
            m_buffer[index * 2 + (i ^ 1)] = static_cast<uint8_t>(i < text.size() ? text[i] : ' ');
    };

    writeWord(0, 0x045a);  // Configuration: fixed disk
    writeWord(1, static_cast<uint16_t>(m_numcylinders));
    writeWord(3, static_cast<uint16_t>(m_numheads));
    writeWord(6, static_cast<uint16_t>(m_numsectors));
    writeText(10, 10, "0000000000");  // Serial number
    writeText(23, 4, "1.0");  // Firmware version
    writeText(27, 20, "UKNCBTL Hard Disk");  // Model
    writeWord(47, 0x8001);  // Read/write multiple support
    writeWord(49, 0x2f00);  // Capabilities: bit9 = LBA
    writeWord(53, 1);  // Words 54-58 are valid
    writeWord(54, static_cast<uint16_t>(m_numcylinders));
    writeWord(55, static_cast<uint16_t>(m_numheads));
    writeWord(56, static_cast<uint16_t>(m_numsectors));
    writeDword(57, totalSectors);  // Current CHS capacity
    writeDword(60, totalSectors);
    writeDword(100, totalSectors);
    if (m_okInverted)
        InvertBuffer(m_buffer);
}

std::expected<long, CHardDrive::AddressError> CHardDrive::CalculateOffset() const
{
    if (m_curcylinder < 0 || m_curcylinder >= m_numcylinders || m_curhead < 0 ||
        m_curhead >= m_numheads || m_cursector < 1 || m_cursector > m_numsectors)
        return std::unexpected(AddressError::invalid_geometry);
    const uint64_t sector = (static_cast<uint64_t>(m_curcylinder) * m_numheads + m_curhead) *
        m_numsectors + (m_cursector - 1);
    const uint64_t byteOffset = sector * IDE_DISK_SECTOR_SIZE;
    if (byteOffset > LONG_MAX)
        return std::unexpected(AddressError::offset_overflow);
    return static_cast<long>(byteOffset);
}

void CHardDrive::ReadNextSector()
{
    m_status |= IDE_STATUS_BUSY;

    m_timeoutcount = TIME_PER_SECTOR * 2;  // Timeout while seek for next sector
    m_timeoutevent = TimeoutEvent::read_sector_done;
}

void CHardDrive::ReadSectorDone()
{
    m_status &= ~IDE_STATUS_BUSY;
    m_status &= ~IDE_STATUS_ERROR;
    m_status |= IDE_STATUS_BUFFER_READY;
    m_status |= IDE_STATUS_SEEK_COMPLETE;

    // Read sector from HDD image to the buffer
    const auto fileOffset = CalculateOffset();
    if (!m_fpFile || !fileOffset || ::fseek(m_fpFile.get(), *fileOffset, SEEK_SET) != 0)
    {
        m_status &= ~IDE_STATUS_BUFFER_READY;
        m_status |= IDE_STATUS_ERROR;
        m_error = IDE_ERROR_BAD_SECTOR;
        return;
    }
    size_t dwBytesRead = ::fread(m_buffer.data(), 1, IDE_DISK_SECTOR_SIZE, m_fpFile.get());
    if (dwBytesRead != IDE_DISK_SECTOR_SIZE)
    {
        m_status &= ~IDE_STATUS_BUFFER_READY;
        m_status |= IDE_STATUS_ERROR;
        m_error = IDE_ERROR_BAD_SECTOR;
        return;
    }

    if (m_sectorcount > 0)
        m_sectorcount--;

    if (m_sectorcount > 0)
    {
        NextSector();
    }

    m_error = IDE_ERROR_NONE;
    m_bufferoffset = 0;
}

void CHardDrive::WriteSectorDone()
{
    m_status &= ~IDE_STATUS_BUSY;
    m_status &= ~IDE_STATUS_ERROR;
    m_status |= IDE_STATUS_BUFFER_READY;
    m_status |= IDE_STATUS_SEEK_COMPLETE;

    // Write buffer to the HDD image
    const auto fileOffset = CalculateOffset();

    //if (fileOffset) DebugLogFormat(_T("WriteSector {:x}\r\n"), *fileOffset);

    if (!m_fpFile || m_okReadOnly || !fileOffset || ::fseek(m_fpFile.get(), *fileOffset, SEEK_SET) != 0)
    {
        m_status &= ~IDE_STATUS_BUFFER_READY;
        m_status |= IDE_STATUS_ERROR;
        m_error = IDE_ERROR_BAD_SECTOR;
        return;
    }

    size_t dwBytesWritten = ::fwrite(m_buffer.data(), 1, IDE_DISK_SECTOR_SIZE, m_fpFile.get());
    if (dwBytesWritten != IDE_DISK_SECTOR_SIZE || ::fflush(m_fpFile.get()) != 0)
    {
        m_status &= ~IDE_STATUS_BUFFER_READY;
        m_status |= IDE_STATUS_ERROR;
        m_error = IDE_ERROR_BAD_SECTOR;
        return;
    }

    if (m_sectorcount > 0)
        m_sectorcount--;

    if (m_sectorcount > 0)
    {
        NextSector();
    }
    else
    {
        m_status &= ~IDE_STATUS_BUFFER_READY;
    }

    m_error = IDE_ERROR_NONE;
    m_bufferoffset = 0;
}

void CHardDrive::NextSector()
{
    // Advance to the next sector, CHS-based
    m_cursector++;
    if (m_cursector > m_numsectors)  // Sectors are 1-based
    {
        m_cursector = 1;
        m_curhead++;
        if (m_curhead >= m_numheads)
        {
            m_curhead = 0;
            m_curcylinder++;
        }
    }
}

void CHardDrive::ContinueRead()
{
    m_bufferoffset = 0;

    m_status &= ~IDE_STATUS_BUFFER_READY;
    m_status &= ~IDE_STATUS_BUSY;

    if (m_sectorcount > 0)
        ReadNextSector();
}

void CHardDrive::ContinueWrite()
{
    m_bufferoffset = 0;

    m_status &= ~IDE_STATUS_BUFFER_READY;
    m_status |= IDE_STATUS_BUSY;

    m_timeoutcount = TIME_PER_SECTOR;
    m_timeoutevent = TimeoutEvent::write_sector_done;
}


//////////////////////////////////////////////////////////////////////
