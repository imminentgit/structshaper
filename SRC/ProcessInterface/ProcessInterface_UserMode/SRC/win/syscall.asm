global do_syscall

section .text
do_syscall:
   ; Setup
   mov r10, rdx                 ; Move first argument into r10

   xor rax, rax                 ; Clear rax
   mov eax, ecx                 ; Move syscall number into rax

   ; Argument remapping
   mov rcx, rdx                 ; Move first argument into rcx
   mov rdx, r8                  ; Move second argument into rdx
   mov r8, r9                   ; Move third argument into r8

   add rsp, 0x8                 ; Move the stack pointer to the first stack argument
   mov r9, [rsp + 0x20]         ; Move fourth argument into r9

   ; fourth arg -> first stack arg
   ; fifth arg -> second stack arg

   ; Perform the syscall
   syscall                      ; Perform the syscall

   sub rsp, 0x8                 ; Move the stack pointer back to the original position
   ret                          ; Return to the caller