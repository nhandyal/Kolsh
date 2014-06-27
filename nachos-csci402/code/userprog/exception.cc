// exception.cc 
//  Entry point into the Nachos kernel from user programs.
//  There are two kinds of things that can cause control to
//  transfer back to here from user code:
//
//  syscall -- The user code explicitly requests to call a procedure
//  in the Nachos kernel.  Right now, the only function we support is
//  "Halt".
//
//  exceptions -- The user code does something that the CPU can't handle.
//  For instance, accessing memory that doesn't exist, arithmetic errors,
//  etc.  
//
//  Interrupts (which can also cause control to transfer from user
//  code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include <iostream>

using namespace std;

int copyin(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes from the current thread's virtual address vaddr.
    // Return the number of bytes so read, or -1 if an error occors.
    // Errors can generally mean a bad virtual address was passed in.
    bool result;
    int n=0;            // The number of bytes copied in
    int *paddr = new int;

    while ( n >= 0 && n < len) {
        result = machine->ReadMem( vaddr, 1, paddr );
        while(!result) { // FALL 09 CHANGES
            result = machine->ReadMem( vaddr, 1, paddr ); // FALL 09 CHANGES: TO HANDLE PAGE FAULT IN THE ReadMem SYS CALL
        } 
      
        buf[n++] = *paddr;
     
        if(!result) {
            //translation failed
            return -1;
        }
        vaddr++;
    }

    delete paddr;
    return len;
}

char* read_into_new_buffer(unsigned int vaddr, int len) {
    char *buf = new char[len+1];            // Kernel buffer to put string in

    if(!buf) {
        return NULL;
    }

    if(copyin(vaddr, len, buf) == -1) {
        printf("%s","Read into buffer received an invalid character array.\n");
        delete buf;
        return NULL;
    }

    // null terminate the string
    buf[len]='\0';

    return buf;
}

int copyout(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes to the current thread's virtual address vaddr.
    // Return the number of bytes so written, or -1 if an error
    // occors.  Errors can generally mean a bad virtual address was
    // passed in.
    bool result;
    int n=0;            // The number of bytes copied in

    while ( n >= 0 && n < len) {
        // Note that we check every byte's address
        result = machine->WriteMem( vaddr, 1, (int)(buf[n++]) );
        if ( !result ) {
            //translation failed
            return -1;
        }
        vaddr++;
    }

    return n;
}

void Create_Syscall(unsigned int vaddr, int len) {
    // Create the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  No
    // way to return errors, though...
    char *buf = new char[len+1];    // Kernel buffer to put the name in

    if (!buf) return;

    if( copyin(vaddr,len,buf) == -1 ) {
        printf("%s","Bad pointer passed to Create\n");
        delete buf;
        return;
    }

    buf[len]='\0';

    fileSystem->Create(buf,0);
    delete[] buf;
    return;
}

int Open_Syscall(unsigned int vaddr, int len) {
    // Open the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  If
    // the file is opened successfully, it is put in the address
    // space's file table and an id returned that can find the file
    // later.  If there are any errors, -1 is returned.
    char *buf = new char[len+1];    // Kernel buffer to put the name in
    OpenFile *f;            // The new open file
    int id;             // The openfile id

    if (!buf) {
        printf("%s","Can't allocate kernel buffer in Open\n");
        return -1;
    }

    if( copyin(vaddr,len,buf) == -1 ) {
        printf("%s","Bad pointer passed to Open\n");
        delete[] buf;
        return -1;
    }

    buf[len]='\0';

    f = fileSystem->Open(buf);
    delete[] buf;

    if(f) {
        if((id = currentThread->space->fileTable.Put(f)) == -1 ) {
            delete f;
        }
        return id;
    }
    else {
        return -1;
    }
}

void Write_Syscall(unsigned int vaddr, int len, int id) {
    // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one. For disk files, the file is looked
    // up in the current address space's open file table and used as
    // the target of the write.
    
    char *buf;      // Kernel buffer for output
    OpenFile *f;    // Open file for output

    if ( id == ConsoleInput) return;
    
    if ( !(buf = new char[len]) ) {
    printf("%s","Error allocating kernel buffer for write!\n");
    return;
    } else {
        if ( copyin(vaddr,len,buf) == -1 ) {
            printf("%s","Bad pointer passed to to write: data not written\n");
            delete[] buf;
            return;
        }
    }

    if ( id == ConsoleOutput) {
      for (int ii=0; ii<len; ii++) {
        printf("%c",buf[ii]);
      }

    } else {
    if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
        f->Write(buf, len);
    } else {
        printf("%s","Bad OpenFileId passed to Write\n");
        len = -1;
    }
    }

    delete[] buf;
}

int Read_Syscall(unsigned int vaddr, int len, int id) {
    // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one.    We reuse len as the number of bytes
    // read, which is an unnessecary savings of space.
    char *buf;      // Kernel buffer for input
    OpenFile *f;    // Open file for output

    if(id == ConsoleOutput){
        return -1;
    }
    
    if(!(buf = new char[len])) {
        printf("%s","Error allocating kernel buffer in Read\n");
        return -1;
    }

    if(id == ConsoleInput) {
        //Reading from the keyboard
        scanf("%s", buf);

        if(copyout(vaddr, len, buf) == -1) {
            printf("%s","Bad pointer passed to Read: data not copied\n");
        }
    }else {
        if((f = (OpenFile *) currentThread->space->fileTable.Get(id))) {
            len = f->Read(buf, len);
            if(len > 0) {
                //Read something from the file. Put into user's address space
                if(copyout(vaddr, len, buf) == -1) {
                    printf("%s","Bad pointer passed to Read: data not copied\n");
                }
            }
        }else {
            printf("%s","Bad OpenFileId passed to Read\n");
            len = -1;
        }
    }

    delete[] buf;
    return len;
}

void Close_Syscall(int fd) {
    // Close the file associated with id fd.  No error reporting.
    OpenFile *f = (OpenFile *) currentThread->space->fileTable.Remove(fd);

    if(f) {
        delete f;
    }else {
        printf("%s","Tried to close an unopen file\n");
    }
}

void Yield_Syscall(){
    printf("Yielding the current thread\n");
    currentThread->Yield();
}

// **************************************************************************************
// LOCK SYSTEM CALLS
// User-level lock operations: Create, Acquire, Release, Delete.
// Input validation is done in target functions
//

int lock_create_syscall(unsigned int vaddr, int length) {
    char* lock_name = read_into_new_buffer(vaddr, length);
    if(lock_name == NULL) {
        lock_name = "Synchronization lock";
    }

    return synchronization_lut->lock_create(lock_name);
}

int lock_acquire_syscall(int lock_index) {
    return synchronization_lut->lock_acquire(lock_index);
}

int lock_release_syscall(int lock_index) {
    return synchronization_lut->lock_release(lock_index);
}

int lock_delete_syscall(int lock_index) {
    return synchronization_lut->lock_delete(lock_index);
}

// **************************************************************************************
// CONDITION INTERFACE
// User-level condition operations: Create, Wait, Signal, Broadcast, Delete.
// Input validation is done in target functions
//

int condition_create_syscall(unsigned int vaddr, int length) {
    char* condition_name = read_into_new_buffer(vaddr, length);
    if(condition_name == NULL) {
        condition_name = "Synchronization condition";
    }

    return synchronization_lut->condition_create(condition_name);
}

int condition_wait_syscall(int condition_index, int lock_index) {
    return synchronization_lut->condition_wait(condition_index, lock_index);
}

int condition_signal_syscall(int condition_index, int lock_index) {
    return synchronization_lut->condition_signal(condition_index, lock_index);
}

int condition_broadcast_syscall(int condition_index, int lock_index) {
    return synchronization_lut->condition_broadcast(condition_index, lock_index);
}

int condition_delete_syscall(int condition_index) {
    return synchronization_lut->condition_delete(condition_index);
}

void print_f_syscall(unsigned int vaddr, int length) {
    char* output_buffer = read_into_new_buffer(vaddr, length);
    printf("%s", output_buffer);
    delete output_buffer;
}

class ThreadData {
public:
    void (*function_to_execute);
    unsigned int stack_start_address;
};

void kernel_fork_boilerplate(int value) {
    printf("\t######   kernel_fork_boilerplate:   ######\n");
    ThreadData *thread_data = (ThreadData *)value;

    // prep the newly forked thread for execution
    machine->WriteRegister(PCReg, (unsigned int)thread_data->function_to_execute);
    machine->WriteRegister(NextPCReg, (unsigned int)thread_data->function_to_execute + 4);
    machine->WriteRegister(StackReg, (unsigned int)thread_data->stack_start_address - 16);
    
    printf("Stack starting point: %d\n", thread_data->stack_start_address);
    printf("Store into PC registers: func = %d\n", (int) thread_data->function_to_execute);
    printf("Store into NextPC registers: func = %d\n", (int) thread_data->function_to_execute + 4);
    
    machine->Run();
}

int Fork_Syscall(void (*func), unsigned int vaddr, int length) {
    printf("Fork_Syscall:\n");
    Thread *new_thread;
    ThreadData *new_thread_data;
    char *thread_name;

    // validate input parameters
    if(currentThread->space->is_invalid_code_address((unsigned int)func) || vaddr <= 0 || length <= 0) {
        printf("@exception.cc\nInvalid input.\n");
        return -1;
    }

    thread_name = read_into_new_buffer(vaddr, length);
    new_thread = new Thread(thread_name);
    new_thread_data = new ThreadData();

    new_thread->space = currentThread->space;

    new_thread_data->function_to_execute = func;
    new_thread_data->stack_start_address = currentThread->space->newStack();

    printf("Forking new thread...\n");
    printf("\tname: [%s]\n", thread_name);
    printf("\tfunc: [0x%p]\n", func);
    
    new_thread->Fork((VoidFunctionPtr) kernel_fork_boilerplate, (int)new_thread_data);
    currentThread->Yield();
    return 0;
}

//----------------------------
//
//
//       Exec SysCall
//
//----------------------------

void exec_kernel_function(int i){
    printf("Inside exec_kernel_function:\n");
    printf("Current Thread: %s\n", currentThread->getName());
    currentThread->space->InitRegisters();     // set the initial register values
    currentThread->space->RestoreState();      // load page table register    
    machine->Run();
}


int exec_syscall(unsigned int vaddr, int size){

    char *buf = new char[size+1];    // Kernel buffer to put the name in
    if (!buf) {
    printf("%s","Can't allocate kernel buffer in Open\n");
    return -1;
    }

    if( copyin(vaddr,size,buf) == -1 ) {
    printf("%s","Bad pointer passed to Open\n");
    delete[] buf;
    return -1;
    }

    buf[size]='\0';
    printf("Characters read from vaddr: %s\n", buf);

    //Open the file name stored in buf
    OpenFile *user_executable = fileSystem->Open(buf);
    if (user_executable == NULL) {
       printf("Unable to open file %s\n", buf);
       return -1;
    }
    //Create a new thread with the address space of the file
    int process_id = process_table->assign_new_process_id();
    if(process_id < 0) {
        printf("The system does not have resources to exec a new process. Aborting.\n");
        return -1;
    }
    AddrSpace *file_space = new AddrSpace(user_executable, process_id);
    process_table->bind_address_space(process_id, file_space);

    Thread *exec_thread = new Thread("exec_thread");
    exec_thread->space = file_space;
    exec_thread->Fork(exec_kernel_function, 0);
    
    delete user_executable;          // close file
    printf("Current Thread: %s\n", currentThread->getName());

    while(!scheduler->getreadyList()->IsEmpty()){
        currentThread->Yield();
    }

    return 0;
}


int Exit_Syscall(int status){
    int number_of_processes = process_table->get_number_of_running_processes();

    if(number_of_processes == 1 /*&& last thread*/){
        interrupt->Halt();
        return 0;
    }
    else if(number_of_processes > 1 /*&& last thread*/){
        // reclaim all memory
        process_table->release_process_id(currentThread->space->get_process_id());
        // reclaim all locks/CVs
        return 0;
    }
    else if(FALSE/*!last thread*/){
        // reclaim 8 stack pages,
        return 0;
    }

    return -1;
}


void ExceptionHandler(ExceptionType which) {
    int type = machine->ReadRegister(2);    // Which syscall?
    int rv = 0;                             // the return value from a syscall

    if ( which == SyscallException ) {
        switch (type) {
            default:
                DEBUG('a', "Unknown syscall - shutting down.\n");
                break;
            case SC_Halt:
                DEBUG('a', "Shutdown, initiated by user program.\n");
                interrupt->Halt();
                break;
            case SC_Create:
                DEBUG('a', "Create syscall.\n");
                Create_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
                break;
            case SC_Open:
                DEBUG('a', "Open syscall.\n");
                rv = Open_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
                break;
            case SC_Write:
                DEBUG('a', "Write syscall.\n");
                Write_Syscall(machine->ReadRegister(4),
                          machine->ReadRegister(5),
                          machine->ReadRegister(6));
                break;
            case SC_Read:
                DEBUG('a', "Read syscall.\n");
                rv = Read_Syscall(machine->ReadRegister(4),
                          machine->ReadRegister(5),
                          machine->ReadRegister(6));
                break;
            case SC_Close:
                DEBUG('a', "Close syscall.\n");
                Close_Syscall(machine->ReadRegister(4));
                break;
            case SC_Yield:
                DEBUG('a', "Yield Syscall.\n");
                Yield_Syscall();
                break;
            case SC_Fork:
                DEBUG('a', "Fork.\n");
                rv = Fork_Syscall( (void*) (machine->ReadRegister(4)), 
                                                machine->ReadRegister(5),
                                                machine->ReadRegister(6));
                break;
            case SC_Exec:
                DEBUG('a',"Exec.\n");
                rv = exec_syscall(machine->ReadRegister(4),machine->ReadRegister(5));
                break; 
            case SC_Exit:
                DEBUG('a', "Print F sys call.\n"); 
                rv = Exit_Syscall(machine->ReadRegister(4));
                break;  
            // LOCK SYSTEM CALLS
            case SC_LOCK_CREATE:
                DEBUG('a', "Lock create syscall.\n");
                rv = lock_create_syscall(machine->ReadRegister(4),
                                            machine->ReadRegister(5));
                break;
            case SC_LOCK_ACQUIRE:
                DEBUG('a', "Lock acquire syscall.\n");
                rv = lock_acquire_syscall(machine->ReadRegister(4));
                break;      
            case SC_LOCK_RELEASE:
                DEBUG('a', "Lock release syscall.\n");
                rv = lock_release_syscall(machine->ReadRegister(4));
                break;
            case SC_LOCK_DELETE:
                DEBUG('a', "Lock delete syscall.\n");
                rv = lock_delete_syscall(machine->ReadRegister(4));
                break;
            // CONDITION SYSTEM CALLS
            case SC_CONDITION_CREATE:
                DEBUG('a', "Condition create syscall.\n");
                rv = condition_create_syscall(machine->ReadRegister(4),
                                                machine->ReadRegister(5));
                break;
            case SC_CONDITION_WAIT:
                DEBUG('a', "Condition wait syscall.\n");
                rv = condition_wait_syscall(machine->ReadRegister(4),
                                                machine->ReadRegister(5));
                break;
            case SC_CONDITION_SIGNAL:
                DEBUG('a', "Condition signal syscall.\n");
                rv = condition_signal_syscall(machine->ReadRegister(4),
                                                machine->ReadRegister(5));
                break;
            case SC_CONDITION_BROADCAST:
                DEBUG('a', "Condition broadcast syscall.\n");
                rv = condition_broadcast_syscall(machine->ReadRegister(4),
                                                    machine->ReadRegister(5));
                break;
            case SC_CONDITION_DELETE:
                DEBUG('a', "Condition delete syscall.\n");
                rv = condition_delete_syscall(machine->ReadRegister(4));
                break;
            case SC_Print_F:
                DEBUG('a', "Print F sys call.\n");
                print_f_syscall(machine->ReadRegister(4),
                                    machine->ReadRegister(5));
                break;
        }
        // Put in the return value and increment the PC
        machine->WriteRegister(2,rv);
        machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg,machine->ReadRegister(PCReg)+4);
        return;
    }else if(which == PageFaultException) {
        printf("UserMode Exception: [PageFaultException]\n");
        interrupt->Halt();
    }else if(which == ReadOnlyException) {
        printf("UserMode Exception: [ReadOnlyException]\n");
        interrupt->Halt();
    }else if(which == BusErrorException) {
        printf("UserMode Exception: [BusErrorException]\n");
        interrupt->Halt();
    }else if(which == AddressErrorException) {
        printf("UserMode Exception: [AddressErrorException]\n");
        interrupt->Halt();
    }else {
        printf("Unexpected user mode exception - which:[%d] type:[%d]\n", which, type);
        interrupt->Halt();
    }
}
