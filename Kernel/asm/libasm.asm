GLOBAL getSeconds
GLOBAL getMinutes
GLOBAL getHour
GLOBAL getDayOfMonth
GLOBAL getMonth
GLOBAL getYear
GLOBAL inb
GLOBAL outb
GLOBAL acquire
GLOBAL release

section .text

; void acquire(uint64_t *lock)
; Spinlock test-and-set usando xchg (atomico/lockeado por hardware sobre memoria).
; Protege la seccion critica de los semaforos contra race conditions.
acquire:
.spin:
	mov rax, 1
	xchg rax, [rdi]   ; intercambio atomico: rax = valor viejo, [rdi] = 1
	test rax, rax
	jnz .spin         ; si estaba tomado (viejo != 0), reintentar
	ret

; void release(uint64_t *lock)
release:
	mov qword [rdi], 0
	ret
	
getSeconds:
	mov al, 0
	out 0x70, al
	in al, 0x71
	ret

getMinutes:
	mov al, 2
	out 0x70, al
	in al, 0x71
	ret

getHour:
	mov al, 4
	out 0x70, al
	in al, 0x71
	ret

getMonth:
	mov al, 8
	out 0x70, al
	in al, 0x71
	ret

getYear:
	mov al, 9
	out 0x70, al
	in al, 0x71
	ret

getDayOfMonth:
	mov al, 7
	out 0x70, al
	in al, 0x71
	ret

outb:
	push rbp
	mov rbp, rsp

	mov dx, di
	mov al, sil
	out dx, al

	pop rbp
	ret

inb:
	push rbp	
	mov rbp, rsp

	mov dx, di 
	in al, dx 

	pop rbp
	ret