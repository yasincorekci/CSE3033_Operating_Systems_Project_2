// Grade is 96
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TRUE				1		// True is 1
#define FALSE				0		// False is 0
#define MAX_LINE			80		// 80 chars per line, per command, should be enough.

#define MAX_HIST_COUNT		10		// Total history buffer size
#define MAX_CMD_LEN			128		// Total command length
#define MAX_BG_COUNT		100		// Maximum background process count

#define FLAG_WRITE			O_CREAT | O_WRONLY | O_TRUNC
#define FLAG_APPEND			O_CREAT | O_WRONLY | O_APPEND
#define FLAG_READ			O_RDONLY

// This array holds history data
char history[MAX_HIST_COUNT][MAX_CMD_LEN];
// Number commands in the history array
int hist_len = 0;

// Hold last command entered
char command[MAX_CMD_LEN];

// Holds background process pids
pid_t bg[MAX_BG_COUNT];
// Current foreground child process pid
pid_t curr_pid = -1;

// Add command to the history
void addHistory(const char *cmd)
{
	int i;

	// Move commands in the history to one next index
	if (hist_len < MAX_HIST_COUNT) {
		for (i = hist_len - 1; i >= 0; i--) {
			snprintf(history[i+1], MAX_CMD_LEN, "%s", history[i]);
		}

		hist_len++;
	}
	else {
		for (i = hist_len - 2; i >= 0; i--) {
			snprintf(history[i+1], MAX_CMD_LEN, "%s", history[i]);
		}
	}

	// New command is in the 0th position in the history
	snprintf(history[0], MAX_CMD_LEN, "%s", cmd);
}

// Print all commands in the history
void printHistory()
{
	int i;

	for (i = 0; i < hist_len; i++) {
		printf("\t%d %s\n", i, history[i]);
	}
}

// Initialize background pid array
void initBackground()
{
	int i;

	for (i = 0; i < MAX_BG_COUNT; i++)
		bg[i] = -1;
}

// Get number of background processes created by parent process (myshell)
int getBackgroudCount()
{
	int i;
	int status;
	int num = 0;
	pid_t pid;

	for (i = 0; i < MAX_BG_COUNT; i++) {
		if (bg[i] != -1) {
			// Check if background child process finished
			if ((pid = waitpid(bg[i], &status, WNOHANG)) == -1) {
				bg[i] = -1;
			}
			else if (pid == 0) {
				// Background child process is not finished
				num++;
			}
			else {
				// Background child process is finished, remove from pid from bg array
				bg[i] = -1;
			}
		}
	}

	return num;
}

// Check if this pid is in the background array
int checkBackgroud(pid_t pid)
{
	int i;

	for (i = 0; i < MAX_BG_COUNT; i++) {
		if (bg[i] == pid)
			return TRUE;
	}

	return FALSE;
}

// Update background processes array
void updateBackground()
{
	int i;
	int status;
	pid_t pid;

	for (i = 0; i < MAX_BG_COUNT; i++) {
		if (bg[i] != -1) {
			// Check if background child process finished
			if ((pid = waitpid(bg[i], &status, WNOHANG)) == -1) {
				bg[i] = -1;
			}
			else if (pid == 0) {
				// Background child process is not finished
			}
			else {
				// Background child process is finished, remove from pid from bg array
				bg[i] = -1;
			}
		}
	}

	return;
}

// Process process to the background process array
void addBackground(pid_t pid)
{
	int i;

	// First update background process array
	updateBackground();

	for (i = 0; i < MAX_BG_COUNT; i++) {
		if (bg[i] == -1) {
			bg[i] = pid;
			return;
		}
	}
}

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

void setup(char inputBuffer[], char *args[], int *background)
{
	int length, /* # of characters in the command line */
	i,      /* loop index for accessing inputBuffer array */
	start,  /* index where beginning of next command parameter is */
	ct;     /* index of where to place the next parameter into args[] */

	ct = 0;

	/* read what the user enters on the command line */
	length = read(STDIN_FILENO,inputBuffer,MAX_LINE);

	/* 0 is the system predefined file descriptor for stdin (standard input),
	 * which is the user's screen in this case. inputBuffer by itself is the
	 * same as &inputBuffer[0], i.e. the starting address of where to store
	 * the command that is read, and length holds the number of characters
	 * read in. inputBuffer is not a null terminated C-string. */

	start = -1;

	if (length == 0)
		exit(0);            /* ^d was entered, end of user command stream */

	/* the signal interrupted the read system call */
	/* if the process is in the read() system call, read returns -1
	 * However, if this occurs, errno is set to EINTR. We can check this  value
	 * and disregard the -1 value */

	if ( (length < 0) && (errno != EINTR) ) {
		perror("error reading the command");
		exit(-1);           /* terminate with error code of -1 */
	}

	snprintf(command, MAX_CMD_LEN, "%s", inputBuffer);

	if (command[length-1] == '\n')
		command[length-1] = '\0';

	// printf(">>%s<<",inputBuffer);

	for (i=0;i<length;i++){ /* examine every character in the inputBuffer */
		switch (inputBuffer[i]){
			case ' ':
			case '\t' :               /* argument separators */
				if(start != -1){
					args[ct] = &inputBuffer[start];    /* set up pointer */
					ct++;
				}
				inputBuffer[i] = '\0'; /* add a null char; make a C string */
				start = -1;
				break;
			case '\n':                 /* should be the final char examined */
				if (start != -1){
					args[ct] = &inputBuffer[start];
					ct++;
				}
				inputBuffer[i] = '\0';
				args[ct] = NULL; /* no more arguments to this command */
				break;
			default :             /* some other character */
				if (start == -1)
					start = i;

				if (inputBuffer[i] == '&'){
					*background  = 1;
					inputBuffer[i-1] = '\0';

				}
		} /* end of switch */
	}    /* end of for */

	if (ct > 0) {
		if (args[ct-1][0] == '&') {
			*background  = TRUE;
			args[ct-1] = NULL;
			return;
		}
	}

	args[ct] = NULL; /* just in case the input line was > 80 */

	//for (i = 0; i <= ct; i++)
	//	printf("args %d = %s\n",i,args[i]);
} /* end of setup routine */

void setup_no_read(char inputBuffer[], char *args[], int *background)
{
	int length, /* # of characters in the command line */
	i,      /* loop index for accessing inputBuffer array */
	start,  /* index where beginning of next command parameter is */
	ct;     /* index of where to place the next parameter into args[] */

	ct = 0;

	length = strlen(inputBuffer);

	/* 0 is the system predefined file descriptor for stdin (standard input),
	 * which is the user's screen in this case. inputBuffer by itself is the
	 * same as &inputBuffer[0], i.e. the starting address of where to store
	 * the command that is read, and length holds the number of characters
	 * read in. inputBuffer is not a null terminated C-string. */

	start = -1;

	if (length == 0)
		exit(0);            /* ^d was entered, end of user command stream */

	/* the signal interrupted the read system call */
	/* if the process is in the read() system call, read returns -1
	 * However, if this occurs, errno is set to EINTR. We can check this  value
	 * and disregard the -1 value */

	if ( (length < 0) && (errno != EINTR) ) {
		perror("error reading the command");
		exit(-1);           /* terminate with error code of -1 */
	}

	inputBuffer[length++] = '\n';

	snprintf(command, MAX_CMD_LEN, "%s", inputBuffer);

	if (command[length-1] == '\n')
		command[length-1] = '\0';

	// printf(">>%s<<",inputBuffer);

	for (i=0;i<length;i++){ /* examine every character in the inputBuffer */
		switch (inputBuffer[i]){
			case ' ':
			case '\t' :               /* argument separators */
				if(start != -1){
					args[ct] = &inputBuffer[start];    /* set up pointer */
					ct++;
				}
				inputBuffer[i] = '\0'; /* add a null char; make a C string */
				start = -1;
				break;
			case '\n':                 /* should be the final char examined */
				if (start != -1){
					args[ct] = &inputBuffer[start];
					ct++;
				}
				inputBuffer[i] = '\0';
				args[ct] = NULL; /* no more arguments to this command */
				break;
			default :             /* some other character */
				if (start == -1)
					start = i;

				if (inputBuffer[i] == '&'){
					*background  = 1;
					inputBuffer[i-1] = '\0';

				}
		} /* end of switch */
	}    /* end of for */

	if (ct > 0) {
		if (args[ct-1][0] == '&') {
			*background  = TRUE;
			args[ct-1] = NULL;
			return;
		}
	}

	args[ct] = NULL; /* just in case the input line was > 80 */

	// for (i = 0; i <= ct; i++)
	//	printf("args %d = %s\n",i,args[i]);

} /* end of setup routine */

// Search command in the PATH executable directories and return full path of the command if command exists
char* parsePath(const char *cmd)
{
	static char path[PATH_MAX];
	char *token;

	// Get path environmental parameter
	char *path_env = getenv("PATH");

	if (path_env != NULL) {
		token = strtok(path_env, ": \r\n");

		while (token != NULL) {
			struct stat _stat;
			snprintf(path, PATH_MAX, "%s/%s", token, cmd);

			// Check if full path exists for this command
			if (stat(path, &_stat) == 0)
				return path;

			token = strtok(NULL, ": \r\n");
		}
	}
	else {
		fprintf(stderr, "Cannot get path environmental variable\n");
	}

	// Executable not found in the paths return the command itself
	snprintf(path, PATH_MAX, "%s", cmd);

	return path;
}

// Signal handler to catch SIGINT and SIGTSTP
void signalHandler(int signum)
{
	signal(SIGINT, signalHandler);
	signal(SIGTSTP, signalHandler);

	if (curr_pid == -1)
		return;


	printf("\n");
	fflush(stdout);

	// Kill current foreground child process
	kill(curr_pid, SIGKILL);
}

// Main method
int main(void)
{
	char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
	int background; /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2 + 1]; /*command line arguments */

	int i;
	int fd;
	int redirect;
	pid_t pid;

	int is_history = FALSE;

	char *path;

	// Initialize background array
	initBackground();

	// Add signal handler for SIGINT and SIGTSTP
	signal(SIGINT, signalHandler);
	signal(SIGTSTP, signalHandler);

	while (TRUE) {
		background = FALSE;

		if (!is_history) {
			// Get input from the user
			printf("myshell: ");
			fflush(stdout);

			memset(inputBuffer, '\0', sizeof(inputBuffer));

			setup(inputBuffer, args, &background);
		}
		else {
			// A history command has been called, parse command without wating for input
			setup_no_read(inputBuffer, args, &background);
			is_history = FALSE;
		}

		if (args[0] == NULL)
			continue;

		/** the steps are:
		 * (1) fork a child process using fork()
		 * (2) the child process will invoke execv()
		 * (3) if background == 0, the parent will wait,
		 * otherwise it will invoke the setup() function again. */

		if (strcmp(args[0], "history") == 0) {
			// history build-in command entered
			if ((args[1] != NULL) && (strcmp(args[1], "-i") == 0) && (args[2] != NULL)) {
				// A command in the history called
				int hid = atoi(args[2]);

				if (hid >= 0 && hid < hist_len) {
					snprintf(inputBuffer, MAX_LINE, "%s", history[hid]);
					is_history = TRUE;
					continue;
				}
			}

			// Print commands in history
			printHistory();
			continue;
		}
		else if (strcmp(args[0], "fg") == 0) {
			// fg build-in command
			if (args[1] != NULL) {
				int num = atoi(args[1]);

				// Check if the process is in the background array
				if (checkBackgroud(num)) {
					int status;

					// Wait this child process to be finished
					do {
						if ((pid = waitpid(num, &status, WNOHANG)) == -1) {
							curr_pid = -1;
						}
						else if (pid == 0) {
							usleep(10000);
						}
						else {
							curr_pid = -1;
						}
					} while (pid == 0);
				}
			}
			else {
				fprintf(stderr, "Usage: fg <process id>\n");
			}

			continue;
		}
		else if (strcmp(args[0], "exit") == 0) {
			// exit build-in command
			// First check if there are still background processes
			int count = getBackgroudCount();

			// If there are background processes don't exit
			if (count == 0) {
				exit(0);
			} else {
				fprintf(stderr, "Background count %d, cannot exit\n", count);
			}

			continue;
		}
		else {
			// Add command to the history
			addHistory(command);
		}

		// Fork and create a child process
		pid = fork();

		if (pid == 0) {
			// Find full path of command with PATH environmental variable
			path = parsePath(args[0]);

			while (args[i] != NULL && args[i+1] != NULL) {
				redirect = TRUE;

				if (strcmp(args[i], ">") == 0) {
					// redirect standard output by creating a new file
					fd = open(args[i+1], FLAG_WRITE, 0664);

					dup2(fd, STDOUT_FILENO);
					close(fd);
				}
				else if (strcmp(args[i], "2>") == 0) {
					// redirect standard error by creating a new file
					fd = open(args[i+1], FLAG_WRITE, 0664);

					dup2(fd, STDERR_FILENO);
					close(fd);
				}
				else if (strcmp(args[i], ">>") == 0) {
					// redirect standard output by appending to the file
					fd = open(args[i+1], FLAG_APPEND, 0664);

					dup2(fd, STDOUT_FILENO);
					close(fd);
				}
				else if (strcmp(args[i], "<") == 0) {
					// redirect standard input from the file
					fd = open(args[i+1], FLAG_READ, 0);

					dup2(fd, STDIN_FILENO);
					close(fd);
				}
				else {
					++i;
					redirect = FALSE;
				}

				// Remove redirect arguments from the arguments list
				if (redirect == TRUE) {
					int k = i + 2;

					while (args[k] != NULL) {
						args[k-2] = args[k];
						++k;
					}

					args[k-2] = NULL;
				}
			}

			// Running execv command
			execv(path, args);

			perror("No such command\n");

			return -1;
		}
		else {
			if (background)
			{
				// Add child process to the background array and don't wait to finish
				printf("[1] %d\n", pid);
				addBackground(pid);
			}
			else
			{
				// Foreground
				int status;
				curr_pid = pid;

				// Wait child process until finished
				do {
					if ((pid = waitpid(curr_pid, &status, WNOHANG)) == -1) {
						perror("wait() error");
					}
					else if (pid == 0) {
						// Child process is not finished
						usleep(10000);
					}
					else {
						// Child process is finished
						curr_pid = -1;
					}
				} while (pid == 0);
			}
		}
	}

	return 0;
}
