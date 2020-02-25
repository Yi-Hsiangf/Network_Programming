#include "np3_shell.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h> //for shared memory
#include <sys/shm.h>

void welcome_msg()
{
    cout << "****************************************" << endl;
    cout << "** Welcome to the information server. **" << endl;
    cout << "****************************************" << endl;
}

