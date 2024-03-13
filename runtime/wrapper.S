// wrapper for klang API 

.intel_syntax noprefix

.extern do_printi
.extern do_prints
.extern do_inputi
.extern do_random 

.section .text, "ax"
.global K_printi
K_printi:
  push rbp 
  mov rbp, rsp 
  and rsp, -16
  mov rdi, qword ptr [rbp + 16]
  call do_printi 
  mov rsp, rbp
  pop rbp
  ret 

.global K_prints
K_prints:
  push rbp 
  mov rbp, rsp 
  and rsp, -16
  mov rdi, qword ptr [rbp + 16]
  call do_prints 
  mov rsp, rbp
  pop rbp
  ret

.global K_inputi
K_inputi:
  push rbp 
  mov rbp, rsp 
  and rsp, -16
  call do_inputi 
  mov rsp, rbp
  pop rbp
  ret

.global K_random
K_random:
  push rbp 
  mov rbp, rsp 
  and rsp, -16
  call do_random 
  mov rsp, rbp
  pop rbp
  ret

.global K_inputs
K_inputs:
  push rbp 
  mov rbp, rsp 
  and rsp, -16
  call do_inputs 
  mov rsp, rbp
  pop rbp
  ret

.global K_array_new
K_array_new:
  push rbp 
  mov rbp, rsp 
  and rsp, -16
  mov rdi, qword ptr [rbp + 16]
  call do_array_new 
  mov rsp, rbp
  pop rbp
  ret

.global K_array_load
K_array_load:
  push rbp 
  mov rbp, rsp 
  and rsp, -16
  mov rdi, qword ptr [rbp + 16]
  mov rsi, qword ptr [rbp + 24]
  call do_array_load 
  mov rsp, rbp
  pop rbp
  ret

.global K_array_store
K_array_store:
  push rbp 
  mov rbp, rsp 
  and rsp, -16
  mov rdi, qword ptr [rbp + 16]
  mov rsi, qword ptr [rbp + 24]
  mov rdx, qword ptr [rbp + 32]
  call do_array_store 
  mov rsp, rbp
  pop rbp
  ret