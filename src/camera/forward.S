/* 自动生成：转发跳板，保持所有参数寄存器不变。 */
    .text
/* 解析失败时的安全回退：返回 0，不崩溃。 */
    .globl camfwd_fallback
    .type camfwd_fallback, %function
camfwd_fallback:
    mov x0, xzr
    ret
    .size camfwd_fallback, .-camfwd_fallback

    .globl _ZN7android10CameraBaseINS_6CameraENS_12CameraTraitsIS1_EEE9getStatusEv
    .type _ZN7android10CameraBaseINS_6CameraENS_12CameraTraitsIS1_EEE9getStatusEv, %function
_ZN7android10CameraBaseINS_6CameraENS_12CameraTraitsIS1_EEE9getStatusEv:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #0]
    br   x16
    .size _ZN7android10CameraBaseINS_6CameraENS_12CameraTraitsIS1_EEE9getStatusEv, .-_ZN7android10CameraBaseINS_6CameraENS_12CameraTraitsIS1_EEE9getStatusEv

    .globl _ZN7android16CameraParameters9unflattenERKNS_7String8E
    .type _ZN7android16CameraParameters9unflattenERKNS_7String8E, %function
_ZN7android16CameraParameters9unflattenERKNS_7String8E:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #8]
    br   x16
    .size _ZN7android16CameraParameters9unflattenERKNS_7String8E, .-_ZN7android16CameraParameters9unflattenERKNS_7String8E

    .globl _ZN7android16CameraParametersC1Ev
    .type _ZN7android16CameraParametersC1Ev, %function
_ZN7android16CameraParametersC1Ev:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #16]
    br   x16
    .size _ZN7android16CameraParametersC1Ev, .-_ZN7android16CameraParametersC1Ev

    .globl _ZN7android16CameraParametersD1Ev
    .type _ZN7android16CameraParametersD1Ev, %function
_ZN7android16CameraParametersD1Ev:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #24]
    br   x16
    .size _ZN7android16CameraParametersD1Ev, .-_ZN7android16CameraParametersD1Ev


    .globl _ZN7android6Camera11setListenerERKNS_2spINS_14CameraListenerEEE
    .type _ZN7android6Camera11setListenerERKNS_2spINS_14CameraListenerEEE, %function
_ZN7android6Camera11setListenerERKNS_2spINS_14CameraListenerEEE:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #40]
    br   x16
    .size _ZN7android6Camera11setListenerERKNS_2spINS_14CameraListenerEEE, .-_ZN7android6Camera11setListenerERKNS_2spINS_14CameraListenerEEE




    .globl _ZN7android6Camera14previewEnabledEv
    .type _ZN7android6Camera14previewEnabledEv, %function
_ZN7android6Camera14previewEnabledEv:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #72]
    br   x16
    .size _ZN7android6Camera14previewEnabledEv, .-_ZN7android6Camera14previewEnabledEv

    .globl _ZN7android6Camera15cancelAutoFocusEv
    .type _ZN7android6Camera15cancelAutoFocusEv, %function
_ZN7android6Camera15cancelAutoFocusEv:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #80]
    br   x16
    .size _ZN7android6Camera15cancelAutoFocusEv, .-_ZN7android6Camera15cancelAutoFocusEv

    .globl _ZN7android6Camera16setPreviewTargetERKNS_2spINS_22IGraphicBufferProducerEEE
    .type _ZN7android6Camera16setPreviewTargetERKNS_2spINS_22IGraphicBufferProducerEEE, %function
_ZN7android6Camera16setPreviewTargetERKNS_2spINS_22IGraphicBufferProducerEEE:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #88]
    br   x16
    .size _ZN7android6Camera16setPreviewTargetERKNS_2spINS_22IGraphicBufferProducerEEE, .-_ZN7android6Camera16setPreviewTargetERKNS_2spINS_22IGraphicBufferProducerEEE

    .globl _ZN7android6Camera23setPreviewCallbackFlagsEi
    .type _ZN7android6Camera23setPreviewCallbackFlagsEi, %function
_ZN7android6Camera23setPreviewCallbackFlagsEi:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #96]
    br   x16
    .size _ZN7android6Camera23setPreviewCallbackFlagsEi, .-_ZN7android6Camera23setPreviewCallbackFlagsEi

    .globl _ZN7android6Camera4lockEv
    .type _ZN7android6Camera4lockEv, %function
_ZN7android6Camera4lockEv:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #104]
    br   x16
    .size _ZN7android6Camera4lockEv, .-_ZN7android6Camera4lockEv

    .globl _ZN7android6Camera6unlockEv
    .type _ZN7android6Camera6unlockEv, %function
_ZN7android6Camera6unlockEv:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #112]
    br   x16
    .size _ZN7android6Camera6unlockEv, .-_ZN7android6Camera6unlockEv

    .globl _ZN7android6Camera9autoFocusEv
    .type _ZN7android6Camera9autoFocusEv, %function
_ZN7android6Camera9autoFocusEv:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #120]
    br   x16
    .size _ZN7android6Camera9autoFocusEv, .-_ZN7android6Camera9autoFocusEv

    .globl _ZN7android6Camera9reconnectEv
    .type _ZN7android6Camera9reconnectEv, %function
_ZN7android6Camera9reconnectEv:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #128]
    br   x16
    .size _ZN7android6Camera9reconnectEv, .-_ZN7android6Camera9reconnectEv

    .globl _ZNK7android16CameraParameters14getPreviewSizeEPiS1_
    .type _ZNK7android16CameraParameters14getPreviewSizeEPiS1_, %function
_ZNK7android16CameraParameters14getPreviewSizeEPiS1_:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #136]
    br   x16
    .size _ZNK7android16CameraParameters14getPreviewSizeEPiS1_, .-_ZNK7android16CameraParameters14getPreviewSizeEPiS1_

    .globl _ZNK7android16CameraParameters3getEPKc
    .type _ZNK7android16CameraParameters3getEPKc, %function
_ZNK7android16CameraParameters3getEPKc:
    adrp x16, g_real_syms
    add  x16, x16, :lo12:g_real_syms
    ldr  x16, [x16, #144]
    br   x16
    .size _ZNK7android16CameraParameters3getEPKc, .-_ZNK7android16CameraParameters3getEPKc
