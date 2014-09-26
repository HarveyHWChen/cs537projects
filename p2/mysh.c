#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <limits.h>

#define CMDL_MAXLEN 512
#define ARGS_MAXNUM 64

/* Declaration of structs */
typedef struct command
{
  char whole_command[CMDL_MAXLEN + 1];
  char* args[ARGS_MAXNUM + 1];
  int instr_type;
  struct command *next;
}Command;

/* Global Variables */
char const error_message[30] = "An error has occurred\n";
char command_line[3 * CMDL_MAXLEN + 1];
Command* cmd_pHeader;
Command* cmd_pCurr;
char tmpline[3 * CMDL_MAXLEN + 1];
char *pRedirect_file;
char redirect_file[PATH_MAX + 1];
char batch_file[PATH_MAX + 1];
int notParallel;
int ifRedirect;
int ifBatch;
FILE* batch_input;
char* const pDelimR = ">";
char* const pDelimP = "|";

/* Declaration of functions */
void print_error(void);
void init(void);
void shell_loop(void);
int read_instr(void);
int read_instr_batch(FILE* input);
int parse_instr(void);
int execute_instr(void);
Command* new_command(char* argsin[], int type, char* command_whole);
void pipe_loop(Command* cmd_Header);


int main(int argc, char * argv[])
{
  if(argc > 2)
    {
      print_error();
      exit(1);
    }
  else if(argc == 1)
    {
      ifBatch = 0;
    }
  else if(argc == 2)
    {
      ifBatch = 1;
      strcpy(batch_file,argv[1]);
    }
  if(ifBatch == 1)
    {
      batch_input = fopen(batch_file,"r");
      if(batch_input == NULL)
	{
	  print_error();
	  exit(1);
	}
    }
  shell_loop();
  return 0;
}

/*
 * prints error message if any error should occur
 */
void print_error(void)
{
  write(STDERR_FILENO, error_message, strlen(error_message));
}

/*
 * initializes variables and memory allocations
 */
void init(void)
{
  /* Detect if batch mode is used */
  memset(command_line, 0, sizeof(command_line));
  memset(redirect_file, 0, sizeof(redirect_file));
  memset(batch_file, 0, sizeof(batch_file));
  memset(tmpline, 0 ,sizeof(tmpline));

  ifRedirect = 0;
}

/*
 * The main loop of this shell
 * Basically this shell is implemented by 3 functions: read_instr()/read_instr_batch(), parse_instr() and execute_instr()
 */
void shell_loop(void)
{
  while(1)
    {
      init();
      if(ifBatch == 0)
	{
	  printf("537sh> ");
	  fflush(stdout);
	  if(read_instr()==-1)
	    break;
	}
      else if(ifBatch == 1)
	{
	  read_instr_batch(batch_input);
	}
      parse_instr();
      execute_instr();
      pid_t pid;
      /* wait for all child processes to finish */
      while((pid = wait(NULL)))
      	{
      	  if(errno == ECHILD)
      	    {
      	      break;
      	    }
      	}
    }
}

/*
 * Read instructions and detect command mode (normal, sequential or parallel)
 * if succeeded, return 0.
 */
int read_instr(void)
{
  if(fgets(command_line, 3*CMDL_MAXLEN, stdin) == NULL)
    return -1;
  return 0;
}

/*
 * Read instruction from input file assigned in batch mode
 */
int read_instr_batch(FILE* input)
{
  if(fgets(command_line, 3*CMDL_MAXLEN, input) == NULL)
    {
      /* if file is empty, just exit */
      exit(0);
    }
  return 0;
}

/*
 * Analyze instructions
 * including checking the legbility of input command; detecting parallel/sequential mode and parsing command line to separate them as individual commands
 */
int parse_instr(void)
{
  //  char *cp = command_line;
  char* cmdp = command_line;
  char *tmpp = tmpline;
  char *tmpargs[ARGS_MAXNUM + 1];
  int i = 0;
  int j = 0;
  int pipe_count = 0;
  int red_count = 0;
  int instrtype = 0;
  char delim1;
  cmd_pHeader = 0;
  cmd_pCurr = 0;


  /* Detect command mode and check the legibility regarding ; and +*/
  char* buf = command_line;
  if(strchr(buf,';') == 0)
    {
      if(strchr(buf,'+') == 0)
	{
	  /*single command*/
	  delim1 = '\n';
	  notParallel = 1;
	}
      else
	{
	  /*parallel command*/
	  delim1 = '+';
	  notParallel = 0;
	}
    }
  else
    {
      if(strchr(buf,'+') == 0)
	{
	  /*sequential command*/
	  delim1 = ';';
	  notParallel = 1;
	}
      else
	{
	  /*input error*/
	  exit(1);
	}
    }

  /* Check the legibility of input regarding | and > */
  /* Check if redirection has been requested no more than once*/
  buf = command_line;  
  int cmdline_len = strlen(command_line);
  if(cmdline_len > 512)
    {
      print_error();
    }
  //int length = 0;  
  while(*buf != '\n')
    {
      if(*buf == '|')
  	pipe_count++;
      buf++;
    }
  buf = command_line;
  while(*buf != '\n')
    {
      if(*buf == '>')
	red_count++;
      buf++;
    }
  if(red_count>1)
    {
      print_error();
    }
  /* Check if redirection is requested before pipeline */
  buf = command_line;
  if(red_count == 1 && pipe_count > 0)
    {
      char* tempptr1 = rindex(buf,'|');
      char* tempptr2 = rindex(buf,'>');
      if(tempptr2 < tempptr1)
	{
	  print_error();
	}
      else
	{
	  ifRedirect = 1;
	}
    }
  else if(red_count == 1)
    {
      ifRedirect = 1;
    }

  if(ifBatch)
    {
      write(STDOUT_FILENO,command_line,strlen(command_line));
    }

  /* Parse Command Line*/
  char* bufptr;
  char command_whole[CMDL_MAXLEN + 1];
  bufptr = strtok(cmdp, &delim1);
  tmpp = tmpline;
  memset(tmpline, 0 ,sizeof(tmpline));
  while(bufptr != 0)
    {
      strcpy(command_whole,bufptr);
      i = 0;
      for(j = 0; j<=ARGS_MAXNUM; j++)
	{
	  tmpargs[j] = 0;
	}
      while(*bufptr != '\0')
	{
	  /* Get rid of the spaces and tables before instructions */
	  while(*bufptr == ' ' || *bufptr == '\t')
	    bufptr++;
	  /* Determine if instruction has ended */
	  if(*bufptr == '\0' || *bufptr == '\n')
	    break;
	  tmpargs[i] = tmpp;
	  
	  /* Analyze aguments */
	  while(*bufptr != '\0' && *bufptr != ' ' && *bufptr != '\t' && *bufptr != '\n')
	    {
	      *tmpp = *bufptr;
	      tmpp++;
	      bufptr++;
	    }
	  *tmpp++ = '\0';
	  i++;
	}
     
      if(tmpargs[0] == 0)
	return -1;

      /* Determine which built-in command to use*/
      if(strcmp(tmpargs[0],"quit") == 0)
	instrtype = 1;
      else if(strcmp(tmpargs[0],"cd") == 0)
	instrtype = 2;
      else if(strcmp(tmpargs[0],"pwd") == 0)
	instrtype = 3;
      else
	instrtype = 0;

      if(cmd_pHeader == 0)
	{
	  cmd_pHeader = new_command(tmpargs, instrtype, command_whole);
	  cmd_pCurr = cmd_pHeader;
	}      
      else
	{
	  cmd_pCurr->next = new_command(tmpargs, instrtype, command_whole);
	  cmd_pCurr = cmd_pCurr->next;
	}
      bufptr = strtok(NULL, &delim1);
    }
  
  return 0;
}

/*
 * Execute instructions
 * supports pipeline mode and redirection mode
 */
int execute_instr(void)
{
  char* cmdp;
  char* buf;
  int i;
  int j;
  int s_fd = 0;
  int n_fd = 0;
  Command* cmd_new;
  int thisRedirect = 0;
  
  /* Execute commands */
  cmd_pCurr = cmd_pHeader;
  while(cmd_pCurr!=NULL)
    {
      /* Check if current command line requests pipeline*/
      int thisPipe = 0;
      if(strchr(cmd_pCurr->whole_command,'|') != 0)
	{
	  thisPipe = 1;
	}
      /* Check if current command line requests redirection*/
      if(strchr(cmd_pCurr->whole_command,'>') != 0)
	{
	  thisRedirect = 1;
	}
      /* get redirection file name */
      cmdp = strtok(cmd_pCurr->whole_command, pDelimR);
      if(cmdp == NULL)
	{
	  exit(0);
	}
      buf = strtok(NULL, pDelimR);
      if(thisRedirect)
	{
	  char* tmpp2 = tmpline;
	  memset(tmpline, 0 ,sizeof(tmpline));
	  //tmpp = tmpline;
	  //redirect_file;
	  while(buf != '\0')
	    {
	      /* Get rid of the spaces and tables before instructions */
	      while(*buf == ' ' || *buf == '\t')
		buf++;
	      /* Determine if instruction has ended */
	      if(*buf == '\0' || *buf == '\n')
		break;
	      pRedirect_file = tmpp2;
	      //char* tmpp = tmpline;
	      while(*buf != '\0' && *buf != ' ' && *buf != '\t' && *buf != '\n')
		{
		  *tmpp2 = *buf;
		  tmpp2++;
		  buf++;
		}
	      *tmpp2 = '\0';
	    }
	  //tmpp = tmpline;
	  strcpy(redirect_file,pRedirect_file);
	  /* Get command before redirection*/
	  cmdp = strtok(cmd_pCurr->whole_command, pDelimR); 
	  if(cmdp == NULL)
	    {
	      print_error();
	      break;
	    }
	  else
	    {
	      /* Make a new command! */
	      char command_whole[CMDL_MAXLEN + 1];
	      char* bufptr;
	      int instrtype = 0;
	      char* tmpargs[ARGS_MAXNUM];
	      char* tmpp = tmpline;
	      //Command* cmd_new;
	      memset(tmpline, 0, sizeof(tmpline));
	      bufptr = strtok(cmd_pCurr->whole_command, pDelimP);
	      while(bufptr != 0)
		{
		  strcpy(command_whole,bufptr);
		  i = 0;
		  for(j = 0; j<=ARGS_MAXNUM; j++)
		    {
		      tmpargs[j] = 0;
		    }
		  while(*bufptr != '\0')
		    {
		      /* Get rid of the spaces and tables before instructions */
		      while(*bufptr == ' ' || *bufptr == '\t')
			bufptr++;
		      /* Determine if instruction has ended */
		      if(*bufptr == '\0' || *bufptr == '\n')
			break;
		      tmpargs[i] = tmpp;
		      /* Analyze aguments */
		      while(*bufptr != '\0' && *bufptr != ' ' && *bufptr != '\t' && *bufptr != '\n')
			{
			  *tmpp = *bufptr;
			  tmpp++;
			  bufptr++;
			}
		      *tmpp++ = '\0';
		      i++;
		    }
		  
		  if(tmpargs[0] == 0)
		    return -1;
		  
		  /* Determine which built-in command to use*/
		  if(strcmp(tmpargs[0],"quit") == 0)
		    instrtype = 1;
		  else if(strcmp(tmpargs[0],"cd") == 0)
		    instrtype = 2;
		  else if(strcmp(tmpargs[0],"pwd") == 0)
		    instrtype = 3;
		  else
		    instrtype = 0;
		  
		  /* cmd_new = new_command(tmpargs,instrtype,command_whole); */
		  /* cmd_new->next = cmd_pCurr->next; */
		  cmd_new = cmd_pCurr->next;
		  cmd_pCurr = new_command(tmpargs,instrtype,command_whole);
		  bufptr = strtok(NULL, pDelimP);		
		}
	    }
	}

      if(thisPipe)
	{
	  /* if pipeline mode is requested in this set of command...*/
	 
	  /* parse command again*/
	  char command_whole[CMDL_MAXLEN + 1];
	  char* bufptr;
	  int instrtype = 0;
	  char* tmpargs[ARGS_MAXNUM];
	  char* tmpp = tmpline;
	  Command* cmd_innerCurr;
	  Command* cmd_innerHeader;
	  memset(tmpline, 0, sizeof (tmpline));
	  bufptr = strtok(cmd_pCurr->whole_command, pDelimP);
	  while(bufptr != 0)
	    {
	      strcpy(command_whole,bufptr);
	      i = 0;
	      for(j = 0; j<=ARGS_MAXNUM; j++)
	  	{
	  	  tmpargs[j] = 0;
	  	}
	      while(*bufptr != '\0')
		{
		  /* Get rid of the spaces and tables before instructions */
		  while(*bufptr == ' ' || *bufptr == '\t')
		    bufptr++;
		  /* Determine if instruction has ended */
		  if(*bufptr == '\0' || *bufptr == '\n')
		    break;
		  tmpargs[i] = tmpp;
		  /* Analyze aguments */
		  while(*bufptr != '\0' && *bufptr != ' ' && *bufptr != '\t' && *bufptr != '\n')
		    {
		      *tmpp = *bufptr;
		      tmpp++;
		      bufptr++;
		    }
		  *tmpp++ = '\0';
		  i++;
		}
	      
	      if(tmpargs[0] == 0)
		return -1;
	      
	      /* Determine which built-in command to use*/
	      if(strcmp(tmpargs[0],"quit") == 0)
		instrtype = 1;
	      else if(strcmp(tmpargs[0],"cd") == 0)
		instrtype = 2;
	      else if(strcmp(tmpargs[0],"pwd") == 0)
		instrtype = 3;
	      else
		instrtype = 0;
	      
	      if(cmd_innerHeader == 0)
		{
		  cmd_innerHeader = new_command(tmpargs, instrtype, command_whole);
		  cmd_innerCurr = cmd_innerHeader;
		}      
	      else
		{
		  cmd_innerCurr->next = new_command(tmpargs, instrtype, command_whole);
		  cmd_innerCurr = cmd_innerCurr->next;
		}
	      bufptr = strtok(NULL, pDelimP);
	    }
	  cmd_innerCurr = cmd_innerHeader;
	  /* Start to execute pipeline commands */
	  pipe_loop(cmd_innerCurr);
	}
      else
	{
	  int redirect_tgt = 0;
	  if(thisRedirect)
	    {
	      redirect_tgt = open(redirect_file,O_RDWR|O_CREAT|O_TRUNC);
	      chmod(redirect_file,S_IWRITE|S_IREAD|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	      if(redirect_tgt == -1)
		{
		  print_error();
		}
	      s_fd = dup(1);
	      n_fd = dup2(redirect_tgt,1);
	      close(redirect_tgt);
	    }
	  /* if not in pipeline or redirect mode */
	  switch(cmd_pCurr->instr_type)
	    {
	    case 0:
	      {
		pid_t pid = fork();
		if(pid == -1)
		  print_error();
		if(pid == 0)
		  execvp(cmd_pCurr->args[0], cmd_pCurr->args);
		if(notParallel == 1)
		  waitpid(pid,NULL,0);
		break;
	      }
	    case 1:
	      exit(0);
	      break;
	    case 2:
	      {
		if(cmd_pCurr->args[1] == 0)
		  {
		    char *buf;
		    buf = getenv("HOME");
		    chdir(buf);
		  }
		else
		  {
		    chdir(cmd_pCurr->args[1]);
		  }
		break;
	      }
	    case 3:
	      {
		if(cmd_pCurr->args[1] != 0)
		  {
		    print_error();
		    break;
		  }
		char buf[PATH_MAX];
		//write(STDOUT_FILENO, getcwd(buf,sizeof(buf)),sizeof(buf));
		printf("%s\n",getcwd(buf,sizeof(buf)));
		fflush(stdout);
		break;
	      }
	    }
	}
      if(thisRedirect)
	{
	  cmd_pCurr = cmd_new;
	}
      else
	{
	  cmd_pCurr = cmd_pCurr->next;
	}
    }
  if(ifRedirect)
    {
      dup2(s_fd, n_fd);
    }
  return 0;
}

/*
 * The constructor of struct Command
 */
Command* new_command(char* argsin[], int type, char* command_whole)
{
  int i;
  Command *thisCmd = (Command*)malloc(sizeof(Command));
  for(i = 0; i<= (ARGS_MAXNUM); i++)
    thisCmd->args[i] = argsin[i];
  thisCmd->next = 0;
  thisCmd->instr_type = type;
  strcpy(thisCmd->whole_command, command_whole);
  return thisCmd;
}

/*
 * The loop supporting pipeline
 */
void pipe_loop(Command* cmd_header)
{
  int p[2];
  Command* cmd_curr;  
  pid_t pid;
  int fd_in = 0;
  cmd_curr = cmd_header;
  while(cmd_curr != NULL)
    {
      pipe(p);
      if((pid=fork()) == -1)
	{
	  print_error();
	}
      else if(pid == 0)
	{
	  dup2(fd_in, 0); //change the input according to the old one 
	  if (cmd_curr->next != NULL)
	    dup2(p[1], 1);
	  close(p[0]);
	  execvp(cmd_curr->args[0],cmd_curr->args);
	  exit(EXIT_FAILURE);
	}
      else
	{
	  wait(NULL);
	  close(p[1]);
	  fd_in = p[0]; //save the input for the next command
	  cmd_curr = cmd_curr->next;		
	}
    }
}
