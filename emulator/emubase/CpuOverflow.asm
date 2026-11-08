; Windows x64 ABI: operands are in CL and DL (or CX and DX), result in EAX.
; These leaf routines touch only volatile registers and need no stack frame.
.code
CpuAddOverflow8 PROC
    add cl, dl
    seto al
    movzx eax, al
    ret
CpuAddOverflow8 ENDP
CpuAddOverflow16 PROC
    add cx, dx
    seto al
    movzx eax, al
    ret
CpuAddOverflow16 ENDP
CpuSubOverflow8 PROC
    sub cl, dl
    seto al
    movzx eax, al
    ret
CpuSubOverflow8 ENDP
CpuSubOverflow16 PROC
    sub cx, dx
    seto al
    movzx eax, al
    ret
CpuSubOverflow16 ENDP
END
