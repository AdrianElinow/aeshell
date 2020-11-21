
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/wait.h>


int num_spaces( char* str, int len );
void repl(bool debug, bool usermode, FILE* source);
int main( int argc, char *argv[] );


int last_index(char* str, int len, char key ){

    int i;

    for( i = len; i >= 0; --i){
        if( str[i] == key )
            return i; 
    }

    return -1;
}

char* slice( char* str, int len, int lo, int hi){

    int i;
    int diff;

    if( hi < lo || lo < 0 || hi > len )
        return str;

    diff = hi - lo;

    char *slice = (char*) calloc(diff, sizeof(char));

    for( i = lo; i < hi; ++i)
        slice[ (i - lo) ] = str[i];

    return slice;

}


int num_spaces( char* str, int len ){

    /*  Counts the number of non-consecutive spaces
    
        Records the number of instances of a space that follow
            a non-space character. Does not count consecutive spaces.
        Essentially functions as a means of counting the number of
            'words' in a given string. 
        Accurate provided that the string does not contain a 
            space-sequence at the head or tail of the given string.
            
     */

    int spaces = 0;
    bool whitespace = false;

    for(int i = 0; i < len; ++i)

        if( str[i] == ' ' ){        // if letter is a space
            if( !whitespace )       // and is non-consecutive
                ++spaces;           // increment counter
            whitespace = true;      // flag that whitespace is entered

        } else whitespace = false;  // or letter is not space, unflag whitespace

    return spaces;
}

void repl(bool debug, bool usermode, FILE* source){

    /* REPL portion of shell program

        Get Input Line
            |
        Outer Tokenizer for discrete commands
            |
        Tokenize discrete command
            | 
        Check for Builtin (path, cd, exit) -> Execute builtin
            | 
        Perform access checks for external command
            | 
        [ fork() -> execvp() -> waitpid() ]  
            | 
        Return to outer tokenizer.
     */

    char error_message[30] = "An error has occurred\n";


    if( debug ) printf("\t[initializing variables]\n");

    // for getLine() to read commands from stdin
    char* content = NULL; // contains text from each line
    size_t contentLength = 0; // pl
    //ssize_t length;

    // strtok() variables
    char* token;
    char* cmd;
    char* outer;
    char* inner;

    // command arguments
    char** args = NULL; 
    int argc = 0;

    // environment path values
    char **path = NULL;
    int pathc = 3;

    char pwd[512];
    char *cd;
    getcwd(pwd, sizeof(pwd));
    cd = slice( pwd, strlen(pwd), last_index(pwd, strlen(pwd), '/'), strlen(pwd));

    /*
    int last_index(char* str, int len, char key ){

    char* slice( char* str, int len, int lo, int hi){
    */


    // preset path to ["", "/usr/bin","/bin"]
    //      This is really __BAD___
    //      Find a better solution later!
    path = (char**) malloc( (pathc*sizeof(char*)) );

    path[0] = (char*) malloc( sizeof("") );
    strcpy(path[0], "");

    path[1] = (char*) malloc( sizeof("/usr/bin") );
    strcpy(path[1], "/usr/bin");
    
    path[2] = (char*) malloc( sizeof("/bin") );
    strcpy(path[2], "/bin");
    

    char* tmp;
    // track child process ID
    int pid;

    if( debug ) printf("\t[Entering REPL]\n");

    while( 1 ){

        free(args);

        // get next command
        if( usermode ) printf("%s >>> ", cd); 
        //length = getline( &content, &contentLength, source );
        getline( &content, &contentLength, source );

        // retrieve command for strtok
        outer = content;
        
        if( debug ) printf("\t[Tokenizing cli input]\n");

        // tokenize commands (sep parallel cmd)
        while( (cmd = strtok_r(outer, "&", &outer) ) ){

            inner = cmd;

            // trim string newline
            if(cmd[strlen(cmd)-1] == '\n')
                cmd[strlen(cmd)-1] = ' ';

            /* calculate memory by
                    counting intra-string spaces to ascertain number
                        of words in the string
                    allocate (words) number of char*'s into args
            */
            argc = num_spaces(cmd, strlen(cmd) );
            args = (char**) malloc( (argc+1) * sizeof(char*) );
            argc = 0;

            // allocate each token to args
            while( (token = strtok_r(inner, " ", &inner) ) ){

                // allocate space for argument
                args[argc] = (char*) malloc(sizeof(token)+1);
                // copy argument from token to args
                strcpy(args[argc], token);

                argc++;

            }

            /*  args must be NULL-terminated to prevent program argument confusion
                Caused commands to alternate between working and failing
                    under same repl state
             */
            args[argc] = NULL;

            tmp = content; // tmp must be set before access() use


            if( debug ) printf("\t[built-in command check]\n");

            // check built-in commands
            /* exit, cd <>, path */
            if( strcmp(args[0], "exit") == 0 ){
                if( debug ) printf("\t[Exiting...]\n");
                exit(0);
            }
        
            if( strcmp(args[0], "cd") == 0 ){
                if( debug ) printf("\t[exec chdir()]\n");

                if(argc == 1){
                    if( usermode) printf("usage: cd <path>\n");
                } else if( argc == 2 ){
                    chdir(args[1]);
                    getcwd(pwd, sizeof(pwd));
                    cd = slice( pwd, strlen(pwd), last_index(pwd, strlen(pwd), '/'), strlen(pwd));

                }else exit(1);

                continue;
            }

            if( strcmp(args[0], "path") == 0 ){

                /*
                    Replace path variables with new paths in args.
                        Forget and reallocate path memory

                 */

                if( debug ) printf("\t[path variable change]\n");
                // clear and resize 'path'
                free(path);
                path = NULL;
                // re-adjust size of path to match args
                path = (char**) realloc(path, (argc * sizeof(*args)) );
                // copy values into 'path'
                memcpy( path, args, (argc * sizeof(*args)) );

                // set path[0] to "" for local executables
                strcpy(path[0], "");

                // path is args, same lengths
                pathc = argc;

                continue;
            }

            if( debug ) printf("\t[Performing access checks] %s %d\n", tmp, pathc);

            for(int i = 1; i < pathc; ++i){

                /* construct access check value (tmp)
                        ex: tmp = "/bin"    + "/" + "ls"
                            tmp = "/usr/bin"+ "/" + "vim"
                 */
                strcpy(tmp, path[i]);
                strcat(tmp, "/");
                strcat(tmp, args[0]);

                if( debug ) printf("\t[access] checking '%s' for X_OK\n", tmp);
                if( access( tmp, X_OK ) != 0 ){
                    if( debug ) printf("\t\t[Fail]\n");
                    continue;
                }
                if( debug ) printf("\t\t[Pass]\n");

            }


            if( debug ) printf("\t[ Forking and Waiting ]\n");
            // fork program and execute command with arguments
            pid = fork();
            if( pid == 0 ){ // child

                execvp( args[0], args );

                // executes only if execv returns (error)
                write(STDERR_FILENO, error_message, strlen(error_message)); 

                exit(0);
            }else{ // parent
                // wait for child task to finish
                wait(&pid);
            }


            if( debug ) printf("\t[REPL iteration completed]\n");

        }

    }

}


int main( int argc, char *argv[]){

    /* aesh.c
        ...
     */

    bool debug = false;
    bool usermode = true;
    FILE* cmdsource;

    for(int i = 1; i < argc; ++i){
        // '-d' flag for debug outputs
        if( strcmp(argv[i], "-d") == 0 )
            debug = true;
        else{ 
            // if filename specified, attempt to open and use as input source.
            if( (cmdsource = fopen(argv[i],"r")) == NULL ){
                // catch error, exit 1
                exit(1);
            }
        }
    }


    /* Assess input source.
            If file is specified in command-line execution, use file as
                command source
            If no file is specified OR file cannot be opened, flag usermode
                and use stdin as command source for interactive mode.  
        
     */
    if( cmdsource != NULL ){
        if(debug) printf("batch mode\n");
        usermode = false;
    }else{
        if(debug) printf("user mode\n");
        cmdsource = stdin;
    }


    // go to repl portion of program
    /*  R ead
        E val
        P rint
        L oop

    */
    repl( debug, usermode, cmdsource );

    return 0;
}



