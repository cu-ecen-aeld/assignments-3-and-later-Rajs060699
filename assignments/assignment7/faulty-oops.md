OOPS Message Analysis</br>
The echo "hello_world" > /dev/faulty command was run which resulted in the following log.</br>

OOPS Message</br>
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000</br>
Mem abort info:</br>
  ESR = 0x96000045</br>
  EC = 0x25: DABT (current EL), IL = 32 bits</br>
  SET = 0, FnV = 0</br>
  EA = 0, S1PTW = 0</br>
  FSC = 0x05: level 1 translation fault</br>
Data abort info:</br>
  ISV = 0, ISS = 0x00000045</br>
  CM = 0, WnR = 1</br>
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042086000</br>
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000</br>
Internal error: Oops: 96000045 [#1] SMP</br>
Modules linked in: hello(O) scull(O) faulty(O)</br>
CPU: 0 PID: 149 Comm: sh Tainted: G           O      5.15.18 #1</br>
Hardware name: linux,dummy-virt (DT)</br>
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)</br>
pc : faulty_write+0x14/0x20 [faulty]</br>
lr : vfs_write+0xa8/0x2a0</br>
sp : ffffffc008d0bd80</br>
x29: ffffffc008d0bd80 x28: ffffff80020f0cc0 x27: 0000000000000000</br>
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000</br>
x23: 0000000040001000 x22: 0000000000000012 x21: 000000555c3efa00</br>
x20: 000000555c3efa00 x19: ffffff8002066600 x18: 0000000000000000</br>
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000</br>
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000</br>
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000</br>
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000</br>
x5 : 0000000000000001 x4 : ffffffc0006f0000 x3 : ffffffc008d0bdf0</br>
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000</br>
Call trace:</br>
 faulty_write+0x14/0x20 [faulty]</br>
 ksys_write+0x68/0x100</br>
 __arm64_sys_write+0x20/0x30</br>
 invoke_syscall+0x54/0x130</br>
 el0_svc_common.constprop.0+0x44/0x100</br>
 do_el0_svc+0x44/0xb0</br>
 el0_svc+0x28/0x80</br>
 el0t_64_sync_handler+0xa4/0x130</br>
 el0t_64_sync+0x1a0/0x1a4</br>
Code: d2800001 d2800000 d503233f d50323bf (b900003f)</br> 
---[ end trace 67f413807e211f71 ]---</br>

Analysis</br>
As seen in the first line of the log: Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000; this error message is caused by the kernel attempting to dereference a NULL pointer.</br>

The mem abort info and data abort info specify the register information at the time of the fault. The line CPU: 0 PID: 159 Comm: sh Tainted: G           O      5.15.18 #1 denotes the CPU on which the fault occurred; the PID and process name causing the OOPS with the PID and Comm field; and the Tainted: 'G' flag specifies that the kernel was tainted by loading a proprietary module.</br>

 pc : faulty_write+0x14/0x20 [faulty] provides the program counter value at the time of the oops. Based on this line, we can deduce that the oops message occurred when executing the faulty_write function. The byte offset at which the crash occurred is 0x14 or 20 bytes into the function while the length of the function is denoted by 0x20 or 32 bytes.</br>

The Link register and stack pointer contents are displayed as well in the subsequent lines. Following these values are the CPU register contents and the call trace. The call trace provides the list of functions that were called before the oops occurred. The  Code:  section provides a hex-dump of the machine code that was executing at the time the oops occurred.</br>
