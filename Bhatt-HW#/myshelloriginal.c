/*
	myshell.c

	Chase Whitener
	FSUID: ccw09c
	CSID: whitener

	Project 2, Linux shell written in C with PThreads
	COP4610 - Operating Systems
	FSU - Spring 2013
*/

/* Turn on GNU goodiness */
#define _GNU_SOURCE

/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

/* Some constants that we will need */
#ifndef LINE_MAX
#define LINE_MAX 65535
#endif
#ifndef PATH_MAX
#define PATH_MAX 1000
#endif
#ifndef MAX_PIDS
#define MAX_PIDS 100
#endif
const char *WHITESPACE = " \n\r\f\t\v";

/* Create our command structure as a type */
typedef struct {
	size_t	argc;
	int		background;
	char	**argv;
	char	*ifile;
	char	*ofile;
} Command;
/* Create our structure for a PID */
typedef struct {
	int pid;
	int jid;
	char command[PATH_MAX];
} MyPid;

/* Rather than pass it around, we're making it global. */
MyPid pids[MAX_PIDS];

/* function prototypes */
void check_pids( void );
void command_add_arg( Command *c, const char *val );
void command_destroy( Command *c );
void command_dump( const Command *c );
void command_init( Command *c );
void command_set_ifile( Command *c, const char *val );
void command_set_ofile( Command *c, const char *val );

void find_exec( Command *c );
int is_directory( const char *path );
int is_file( const char *path );

int		mypid_first_free( void );
void	mypid_init( void );
int		mypid_max_pid( void );
int		mypid_next_jid( void );

void parse_input( const char *path ); /* run command */
void redirect_in( Command *c );
void redirect_out( Command *c );
void run_cd( const Command *c );
void run_echo( const Command *c );
void run_ext( const char *path, Command *c );
void run_jobs( void );
void show_prompt( void ); /* displays the user prompt */

/* -----------------------------------------------------------------------------
 main( void )
 -- control the flow of our application with a while loop
----------------------------------------------------------------------------- */
int main(void) {
	char buffer[LINE_MAX];
	char *command;

	mypid_init();

	while ( 1 ) {
		check_pids();
		show_prompt();
		if ( fgets(buffer,LINE_MAX,stdin) == NULL ) {
			fputs( "Error reading from STDIN.  Exiting.\n", stderr );
			return 1;
		}
		command = strtok( buffer, WHITESPACE );
		if ( command == NULL ) {
			continue;
		}
		if ( strcmp(command, "exit") == 0 ) {
			break;
		}
		else {
			parse_input( command );
		}
	}
	return 0;
}

/* ----------------------------------------------------------------------------
 check_pids()
 -- Clean up after running threads
----------------------------------------------------------------------------- */
void check_pids( void ) {
	pid_t id;
	do {
		id = waitpid( -1, NULL, WNOHANG );
		if ( id > 0 ) {
			for ( size_t i=0; i < MAX_PIDS; ++i ) {
				if ( (int)id == pids[i].pid ) {
					printf( "[%d] Done %s\n", pids[i].jid, pids[i].command );
					pids[i].jid=0;
					pids[i].pid=0;
					pids[i].command[0]='\0';
					break;
				}
			}
		}
	} while ( id > 0 );
}

/* -----------------------------------------------------------------------------
 command_add_arg( Command *c, const char *val )
 -- resize our dynamic array. allocate enough space in the slot to store val
	and copy val's contents into it
	argc is the count of actual arguments we have.  0 - (argc-1) are real args
	argc itself will always be set to null so exec knows what to do
----------------------------------------------------------------------------- */
void command_add_arg( Command *c, const char *val ) {
	char **temp;
	char *tmp;
	if ( NULL == c ) return;
	if ( NULL == val ) return;
	if ( strlen(val)==0 ) return;

	if ( (temp = realloc( c->argv, sizeof(char*)*(c->argc+2))) == NULL ) {
		fputs("Failed to re-size argument array.\n", stderr);
		command_destroy( c );
		exit(1);
	}
	c->argv = temp;
	c->argc++;
	temp = NULL;

	if ( (tmp = malloc( sizeof(char)*(strlen(val)+1) ))==NULL) {
		fputs( "Failed to allocate memory for string argument.\n", stderr );
		command_destroy( c );
		exit(1);
	}
	strcpy( tmp, val );
	c->argv[c->argc - 1] = tmp;
	c->argv[c->argc] = NULL;
	tmp = NULL;
}

/* -----------------------------------------------------------------------------
 command_destroy( Command *c )
 -- Clean up any allocated memory for our Command struct
----------------------------------------------------------------------------- */
void command_destroy( Command *c ) {
	size_t i;
	if ( NULL == c ) return;
	if ( NULL != c->ifile ) free( c->ifile );
	if ( NULL != c->ofile ) free( c->ofile );
	if ( NULL != c->argv ) {
		for ( i = 0; i < c->argc; ++i ) {
			if ( NULL != c->argv[i] ) free(c->argv[i]);
			c->argv[i] = NULL;
		}
		free(c->argv);
	}
	c->ifile = NULL;
	c->ofile = NULL;
	c->argv = NULL;
	c->background = 0;
	c->argc = 0;
}

/* -----------------------------------------------------------------------------
 command_dump( const Command *c )
 -- Dump out the information stored in our Command.  Helps to debug
----------------------------------------------------------------------------- */
void command_dump( const Command *c ) {
	size_t i;
	printf( "\n\n" );
	printf( "------------------------------------\n" );
	if ( NULL == c ) {
		printf( " Command is null\n" );
		printf( "------------------------------------\n" );
		return;
	}
	if ( c->argc >= 1 ) printf( " Command: `%s`\n", c->argv[0] );
	printf( "  Backgrounded? %d\n", c->background );
	printf( "  Input File: %s\n  Output File: %s\n", c->ifile, c->ofile );
	printf( "  Num Args: %lu\n", c->argc );
	for (i=0; i <= c->argc; ++i ) {
		printf( "   arg[%lu] = %s\n", i, c->argv[i] );
	}
	printf( "------------------------------------\n\n" );
}

/* -----------------------------------------------------------------------------
 command_init( Command *c )
 -- Initialize our Command struct and make sure we always have one argument
	set to NULL
----------------------------------------------------------------------------- */
void command_init( Command *c ) {
	if ( NULL == c ) return;
	c->ifile = NULL;
	c->ofile = NULL;
	c->argv = NULL;
	c->argc = 0;
	c->background = 0;
	if ( (c->argv=malloc(sizeof(char*)))==NULL ) {
		fputs("failed to allocate memory\n", stderr);
		exit(1);
	}
	c->argv[0] = NULL;
}

/* -----------------------------------------------------------------------------
 command_set_ifile( Command *c, const char *val )
 -- Set the ifile portion to the value supplied.  Properly allocate
 enough space to do so.  If no space can be allocated, destroy the Command
 as best we can and exit.
----------------------------------------------------------------------------- */
void command_set_ifile( Command *c, const char *val ) {
	char *temp;
	if ( NULL == c ) return;
	if ( NULL == val ) return;
	if ( strlen(val)==0 ) return;

	if ( (temp = realloc( c->ifile, sizeof(char)*(strlen(val)+1)))==NULL ) {
		fputs( "Failed to adjust the size available for input file.\n", stderr );
		command_destroy( c );
		exit(1);
	}
	strcpy( temp, val );
	c->ifile = temp;
}

/* -----------------------------------------------------------------------------
 command_set_ofile( Command *c, const char *val )
 -- Set the path (command) portion to the value supplied.  Properly allocate
 enough space to do so.  If no space can be allocated, destroy the Command
 as best we can and exit.
----------------------------------------------------------------------------- */
void command_set_ofile( Command *c, const char *val ) {
	char *temp;
	if ( NULL == c ) return;
	if ( NULL == val ) return;
	if ( strlen(val)==0 ) return;

	if ( (temp = realloc( c->ofile, sizeof(char)*(strlen(val)+1)))==NULL ) {
		fputs( "Failed to adjust the size available for output file.\n", stderr );
		command_destroy( c );
		exit(1);
	}
	strcpy( temp, val );
	c->ofile = temp;
}

/* ----------------------------------------------------------------------------
 find_exec( MyPid *p, Command *c )
 -- Find out which command we're to run, then pass that off to the run_ext()
----------------------------------------------------------------------------- */
void find_exec( Command *c ) {
	if ( NULL == c ) return;
	if ( c->argc < 1 || NULL==c->argv[0] || 0==strlen(c->argv[0]) ) return;

	if ( strchr(c->argv[0], '/') ) {
		if ( !is_file( c->argv[0] ) ) {
			printf( "%s: Command not found.\n", c->argv[0] );
			return;
		}
		run_ext( c->argv[0], c );
		return;
	}

	char *ENV = strdup( getenv( "PATH" ) ); /* MUST free() this */
	char *temp = strtok( ENV, ":\n" );
	char *path = NULL;
	while ( temp != NULL ) {
		if ( NULL==(path = realloc( path, strlen(temp)+strlen(c->argv[0])+2 )) ) {
			fputs( "Error on space allocation\n", stderr );
			if ( NULL!=ENV ) free(ENV);
			exit(1);
		}
		strcpy( path, temp );
		strcat( path, "/" );
		strcat( path, c->argv[0] );
		if ( is_file(path) ) {
			break;
		}
		temp = strtok( NULL, ":\n");
	}
	if ( NULL!=ENV ) free(ENV);
	if ( !is_file( path ) ) {
		printf( "%s: Command not found.\n", c->argv[0] );
		if ( NULL!=path ) free(path);
		return;
	}
	run_ext( path, c );
	if ( NULL!=path ) free(path);
}

/* ----------------------------------------------------------------------------
 is_directory( const char* path )
 -- if path given is a directory, return 1.  else return 0.
----------------------------------------------------------------------------- */
int is_directory( const char *path ) {
	struct stat fstats;
	int ret = 0;
	if ( NULL==path ) return 0;
	if ( strlen(path) < 1 ) return 0;

	ret = stat( path, &fstats );
	if ( 0 != ret ) {
		return 0;
	}
	if( S_ISDIR(fstats.st_mode) ) return 1;
	return 0;
}

/* ----------------------------------------------------------------------------
 is_file( const char* path )
 -- if path given is a directory, return 1.  else return 0.
----------------------------------------------------------------------------- */
int is_file( const char *path ) {
	struct stat fstats;
	int ret = 0;
	if ( NULL==path ) return 0;
	if ( strlen(path) < 1 ) return 0;

	ret = stat( path, &fstats );
	if ( 0!=ret ) return 0;
	if( S_ISREG(fstats.st_mode) ) return 1;
	return 0;
}

/* ----------------------------------------------------------------------------
 mypid_first_free()
 -- Return the first open slot to add a PID   -1 on error
----------------------------------------------------------------------------- */
int mypid_first_free( void ) {
	/* not really necessary, but whatever */
	for ( int i=0; i < MAX_PIDS; ++i ) {
		if ( 0 == pids[i].pid && 0 == pids[i].jid && 0 == strlen(pids[i].command) )
			return i;
	}
	return -1;
}

/* ----------------------------------------------------------------------------
 mypid_init( MyPid *p )
 -- destroy a PID struct if needed
----------------------------------------------------------------------------- */
void mypid_init( void ) {
	/* not really necessary, but whatever */
	for ( size_t i=0; i < MAX_PIDS; ++i ) {
		pids[i].pid = 0;
		pids[i].jid = 0;
		pids[i].command[0] = '\0';
	}
}

/* ----------------------------------------------------------------------------
 mypid_next_jid( void )
 -- Return the next available JID
----------------------------------------------------------------------------- */
int mypid_next_jid( void ) {
	int next = 1;
	do {
		int found = 0;
		for ( size_t i = 0; i < MAX_PIDS; ++i ) {
			if ( pids[i].jid == next ) found = 1;
		}
		if ( !found ) break;
		++next;
	} while ( next <= MAX_PIDS );
	return next;
}

/* ----------------------------------------------------------------------------
 mypid_max_pid()
 -- Return the highest number of a PID we have
----------------------------------------------------------------------------- */
int mypid_max_pid( void ) {
	int max = 0;
	/* not really necessary, but whatever */
	for ( size_t i=0; i < MAX_PIDS; ++i ) {
		if ( pids[i].pid > max ) max = pids[i].pid;
	}
	return max;
}

/* -----------------------------------------------------------------------------
 parse_input( MyPid *pids, const char *path )
 -- figure out what command we're supposed to try and do it
 parse out the buffer as the arguments to the command with an extra NULL
----------------------------------------------------------------------------- */
void parse_input( const char *path ) {
	Command com;
	char *token;
	if ( path == NULL || strlen(path)==0 ) return;

	/* setup a new Command */
	command_init( &com );
	command_add_arg( &com, path );

	token = strtok( NULL, WHITESPACE );
	while ( token != NULL ) {
		if ( strlen(token) > 0 ) {
			if ( strcmp(token, ">") == 0 ) {
				token = strtok( NULL, WHITESPACE );
				if ( token == NULL ) {
					fputs( "Invalid output file.\n", stderr );
					command_destroy( &com );
					return;
				}
				command_set_ofile( &com, token );
			}
			else if ( strcmp(token, "<") == 0 ) {
				token = strtok( NULL, WHITESPACE );
				if ( token == NULL ) {
					fputs( "Invalid input file.\n", stderr );
					command_destroy( &com );
					return;
				}
				command_set_ifile( &com, token );
			}
			else if ( strcmp(token,"&")==0 ) {
				com.background = 1;
			}
			else {
				if ( strlen(token) > 1 && token[0]=='$' ) {
					char *temp = getenv( token+1 );
					if ( temp == NULL ) {
						printf( "%s: Undefined variable.\n", token );
						command_destroy( &com );
						return;
					}
					command_add_arg( &com, temp );
				}
				else {
					command_add_arg( &com, token );
				}
			}
		}
		token = strtok( NULL, WHITESPACE );
	}

	if ( strcmp(path,"echo")==0 ) {
		run_echo( &com );
	}
	else if ( strcmp(path,"cd")==0 ) {
		run_cd( &com );
	}
	else if ( strcmp(path,"jobs")==0 ) {
		run_jobs();
	}
	else {
		find_exec( &com );
	}

	command_destroy( &com );
}

/* ----------------------------------------------------------------------------
 redirect_in( const Command *c )
 -- redirect input to come from a file.  exit() on error
----------------------------------------------------------------------------- */
void redirect_in( Command *c ) {
	if ( NULL == c ) return;
	if ( NULL == c->ifile ) return;

	if ( !is_file( c->ifile ) ) {
		fprintf( stderr, "%s: No such file or directory.\n", c->ifile );
		command_destroy( c );
		exit(EXIT_FAILURE);
	}
	int status = open( c->ifile, O_RDONLY );
	if ( status == -1 ) {
		fprintf( stderr, "%s: No such file or directory.\n", c->ifile );
		command_destroy( c );
		exit(EXIT_FAILURE);
	}
	dup2(status,0);
	close(status);
}

/* ----------------------------------------------------------------------------
 redirect_out( const Command *c )
 -- redirect output to go to a file.  exit() on error
----------------------------------------------------------------------------- */
void redirect_out( Command *c ) {
	if ( NULL == c ) return;
	if ( NULL == c->ofile ) return;
	int status = open(c->ofile, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
	if (status == -1) {
		printf("%s: No such file or directory.\n", c->ofile);
		command_destroy( c );
		exit(EXIT_FAILURE);
	}
	dup2(status,1);
	close(status);
}

/* ----------------------------------------------------------------------------
 run_cd( const Command *c )
 -- Mimic the "cd" command on your normal shell
----------------------------------------------------------------------------- */
void run_cd( const Command *c ) {
	if ( NULL == c ) return;

	if ( c->argc > 2 ) {
		printf( "cd: Too many arguments.\n" );
		return;
	}

	char *path = getenv("HOME");
	if ( c->argc == 2 ) path = c->argv[1];

	if ( 0 == chdir( path ) ) return;

	/* uh, oh.  We made it here, so there was a problem! */
	printf( "%s: No such file or directory.\n", path );
	return;
}

/* ----------------------------------------------------------------------------
 run_echo( const Command *c )
 -- Mimic the "echo" command on your normal shell
----------------------------------------------------------------------------- */
void run_echo( const Command *c ) {
	size_t i;
	if ( NULL == c ) return;

	for ( i = 1; i < c->argc; ++i ) {
		if ( i > 1 ) printf( " " );
		printf( "%s", c->argv[i] );
	}
	printf("\n");
}

/* ----------------------------------------------------------------------------
 run_ext( const char *path, MyPid *p, Command *c )
 -- run an external command
----------------------------------------------------------------------------- */
void run_ext( const char *path, Command *c ) {
	if ( NULL==c ) return;
	//printf("Running: %s\n", path);

	pid_t pid = fork();
	if ( pid==0 ) {
		/* Only child executes */
		redirect_out(c);
		redirect_in(c);
		execv( path, c->argv );
		exit(0);
	}
	else if ( pid < 0 ) {
		/* Failed for fork */
		fputs( "Failed to fork.  Please try again.\n", stderr );
		return;
	}
	else {
		/* only the parent. */
		if ( c->background ) {
			int index = mypid_first_free();
			pids[index].jid = mypid_next_jid();
			pids[index].pid = (int)pid;
			for ( size_t i = 0; i < c->argc; ++i ) {
				if ( i > 0 ) strcat( pids[index].command, " " );
				strcat( pids[index].command, c->argv[i] );
			}
			printf( "[%d] %d\n\n", pids[index].jid, pids[index].pid );
		}
		else {
			waitpid(pid, 0, 0);
		}
		return;
	}
}

/* ----------------------------------------------------------------------------
 run_jobs( )
 -- print out our jobs
----------------------------------------------------------------------------- */
void run_jobs( void ) {
	for ( size_t i = 0; i < MAX_PIDS; ++i ) {
		if ( 0 != pids[i].pid && 0 != pids[i].jid && 0 != strlen(pids[i].command) ) {
			printf( "[%d] %d %s\n", pids[i].jid, pids[i].pid, pids[i].command );
		}
	}
	check_pids();
	printf("\n");
}

/* ----------------------------------------------------------------------------
 show_prompt()
 -- Shows a prompt  USER@myshell:CWD>
----------------------------------------------------------------------------- */
void show_prompt( void ) {
	char *cwd = get_current_dir_name(); /* you HAVE to free() this */
	printf( "%s@myshell:%s> ", getenv("USER"), cwd );
	if ( NULL != cwd ) free(cwd);
	cwd = NULL;
}
