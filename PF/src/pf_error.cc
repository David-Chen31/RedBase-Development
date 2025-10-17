//
// pf_error.cc - PF Component Error Handling
//

#include <iostream>
#include <string>
#include "pf.h"

using namespace std;

//
// PF_PrintError
//
// Desc: Send a message corresponding to a PF return code to cerr
// In:   rc - return code for which a message is desired
//
void PF_PrintError(RC rc)
{
    if (rc == 0) {
        cerr << "PF_PrintError called with return code of 0\n";
        return;
    }
    
    // Positive error codes (warnings/recoverable errors)
    switch (rc) {
        case PF_EOF:
            cerr << "PF warning: end of file\n";
            break;
        case PF_PAGEPINNED:
            cerr << "PF warning: page pinned in buffer\n";
            break;
        case PF_PAGENOTINBUF:
            cerr << "PF warning: page not in buffer\n";
            break;
        case PF_PAGEUNPINNED:
            cerr << "PF warning: page already unpinned\n";
            break;
        case PF_PAGEFREE:
            cerr << "PF warning: page already free\n";
            break;
        case PF_INVALIDPAGE:
            cerr << "PF warning: invalid page number\n";
            break;
        case PF_FILEOPEN:
            cerr << "PF warning: file already open\n";
            break;
        case PF_CLOSEDFILE:
            cerr << "PF warning: file closed\n";
            break;
            
        // Negative error codes (serious errors)
        case PF_NOMEM:
            cerr << "PF error: out of memory\n";
            break;
        case PF_NOBUF:
            cerr << "PF error: buffer pool full\n";
            break;
        case PF_INCOMPLETEREAD:
            cerr << "PF error: incomplete read from file\n";
            break;
        case PF_INCOMPLETEWRITE:
            cerr << "PF error: incomplete write to file\n";
            break;
        case PF_HDRREAD:
            cerr << "PF error: read header failed\n";
            break;
        case PF_HDRWRITE:
            cerr << "PF error: write header failed\n";
            break;
        case PF_PAGEINBUF:
            cerr << "PF error: page already in buffer\n";
            break;
        case PF_HASHNOTFOUND:
            cerr << "PF error: hash table entry not found\n";
            break;
        case PF_HASHPAGEEXIST:
            cerr << "PF error: page already exists in hash table\n";
            break;
        case PF_INVALIDNAME:
            cerr << "PF error: invalid file name\n";
            break;
        case PF_UNIX:
            cerr << "PF error: Unix system call error\n";
            break;
        default:
            cerr << "PF error: unknown error code " << rc << "\n";
            break;
    }
}