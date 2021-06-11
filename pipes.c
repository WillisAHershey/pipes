//Willis A. Hershey
//WillisAHershey@gmail.com
//
//This is a useless demonstration program of how to use pipes over a fork() to forward stdin stdout and stderr
//essentially creating a wrapper program over whatever the child process executes

//This has to be here or gcc complains about kill()
#define _POSIX_C_SOURCE 	1

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

static int pem = 0; //This is just a hacky way to access the child's pid globally

void sigchld_handler(int signal){ //This is called when the parent process is notified its child terminated--it prints a message and terminates
  fprintf(stderr,"Child terminated\n");
  exit(EXIT_SUCCESS);
}

void sigint_handler(int signal){ //This is called when the parent process gets a SIGINT--it sends SIGHUP to child, and otherwise ignores the signal
  printf("\n");			 //When child gets SIGHUP it will terminate which will cause parent to be sent SIGCHLD, so SIGINT still indirectly causes parent to terminate
  kill(pem,SIGHUP);
}

int main(){
  int stdin_pipe[2]; //Create three pipes before the fork
  int stdout_pipe[2];
  int stderr_pipe[2];
  pipe(stdin_pipe);
  pipe(stdout_pipe);
  pipe(stderr_pipe);
  
  int pid = fork(); //We are assuming that fork succeeded (which is not safe)

  if(pid == 0){
	close(stdin_pipe[1]);
	close(stdout_pipe[0]);
	close(stderr_pipe[0]);
	dup2(stdin_pipe[0],STDIN_FILENO); //Redirect stdin stdout and stderr to appropriate ends of appropriate pipes
	close(stdin_pipe[0]);
	dup2(stdout_pipe[1],STDOUT_FILENO);
	close(stdout_pipe[1]);
	dup2(stderr_pipe[1],STDERR_FILENO);
	close(stderr_pipe[1]);
	execlp("/usr/bin/cu","cu","--line","ttyUSB0","--speed","115200","--nostop",(char*)NULL); //Perform some action that requires stdin stdout and stderr
  }
  else{
	pem = pid;
	close(stdin_pipe[0]);
	close(stdout_pipe[1]);
	close(stderr_pipe[1]);
	fcntl(STDIN_FILENO,F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK); //Makes stdin and read-ends of stdout_pipe and stderr_pipe non-blocking for read()
	fcntl(stdout_pipe[0],F_SETFL, fcntl(stdout_pipe[0],F_GETFL) | O_NONBLOCK);
	fcntl(stderr_pipe[0],F_SETFL, fcntl(stderr_pipe[0],F_GETFL) | O_NONBLOCK);
	signal(SIGCHLD,sigchld_handler);	//This is a depreciated way to register signal handlers but the accepted way is super messy so deal
	signal(SIGINT,sigint_handler);
	char in_buf[1024];
	char out_buf[1024];
	char err_buf[1024];
	ssize_t in_len = 0;
	ssize_t out_len = 0;
	ssize_t err_len = 0;
	while(1){
		ssize_t in_ret = read(STDIN_FILENO,in_buf,1024); 		//	These blocks all do basically the same thing--they read from some non-blocking file descriptor
		if(in_ret > 0){							//	and fill their own buffer while mantaining their own length-of-buffer value. When that buffer
			in_len += in_ret;					//	is terminated with a newline character or is full, the read is considered complete, and is handled.
			if(in_buf[in_len-1] == '\n' || in_len == 1024){
				write(stdin_pipe[1],in_buf,in_len);
				in_len = 0;					//	The first block reads from stdin, and when the read is complete, it writes the buffer into a pipe
			}							//	directed to the stdin of the child process
		}
		ssize_t out_ret = read(stdout_pipe[0],out_buf,1024);
		if(out_ret > 0){						//	The other two read from pipes connected to the stdout and stderr of the child process and when
			out_len += out_ret;					//	complete, write them to parent's stdout/stderr with a label indicating which they came from
			if(out_buf[out_len-1] == '\n' || out_len == 1024){
				write(STDOUT_FILENO,"stdout: ",8);
				write(STDOUT_FILENO,out_buf,out_len);
				out_len = 0;
			}
		}
		ssize_t err_ret = read(stderr_pipe[0],err_buf,1024);
		if(err_ret > 0){
			err_len += err_ret;
			if(err_buf[err_len-1] == '\n' || err_len == 1024){
				write(STDERR_FILENO,"stderr: ",8);
				write(STDERR_FILENO,err_buf,err_len);
				err_len = 0;
			}
		}
	}
  }
}
