/*
 * Internal cron with required functionality for IDS cluster
 * addtional function are for task:
 * - run on second period
 * - run on dedicated node
 * 
 * VERSION: 0.6
 *
 * History Version:
 * 0.1 2012 December - created
 * 0.2 2013 January - support to execute binaries
 * 0.3 2013 August - use UID/GID with user name to drop permission without ping.pl script from root account
 * 			-  fix problem with repeated tasks
 * 0.4 2013 August - Add pipe functionality for PHP script
 * 0.5 2013 November - remove pipe functionality (move to separate PERL script), program with script: passed to this perl script by named pipe (/tmp/perl_main-<USER>)
 * 0.6 2014 October - check is pipe_path exist as pipe, if not, not open
 *
 * Copyright by BROWARSKI
 */

#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "wlog.c"
// #define STRFIND 1
// #define ADD_N_I 1
// #define ADD_N 1


/*
 * GLOBAL VARIABLES
 */
/*
 * hostname where we run (defined from arg or take from system)
 */
char ids_name[HOST_NAME_MAX];
/*
 * PID file - for comunication
 */
char pid_file[PATH_MAX] = "/var/tmp/";
/*
 * FIFO to perl main
 */ 
char pipe_path[PATH_MAX] = "/tmp/perl-main";
/*
 * directory with scripts (when main pipe isn't available, then run scrip seperatly
 */
char scripts_dir[PATH_MAX]; 
int pipe_fd = -1;

int16_t *sec[60];
int16_t *min[60];
int16_t *hour[24];
int16_t *day[31];
int16_t *wday[7];
int16_t *tz[2];
char **cmd;

char err_dir[PATH_MAX] = "/var/tmp/";

uint8_t debug = 1;
/*
 * working UID
 */
uid_t wuid = 0;
gid_t wgid = 0;
char w_user[1024];
/*
 * array with pids
 * 0 - main deamon - time engine
 * 1 - entry pipe in /tmp folder
 * 2 - perl main script
 */
// pid_t pid[4];
/*
 * prottypes
 */
void read_fifo(uint16_t);

void open_pipe()
{
	struct stat s;
	/*
	 * check is file exist
	 */
	if (! stat(pipe_path, &s)) {
		// is pipe?
		if (S_ISFIFO(s.st_mode)) {
			/*
			 * open pipe to send request to external PERL engine
			 */	
			pipe_fd = open(pipe_path, O_WRONLY|O_SYNC|O_NONBLOCK);
		        if (pipe_fd < 0) {
		        	WLOG("unable to open pipe %s: %s\n", pipe_path, strerror(errno));
			} else {
				WLOG("pipe open succesfully\n");
			}
		} else {	
			WLOG("%s isn't pipe\n", pipe_path);
		}
	} else {
		WLOG("%s doesn't exist\n", pipe_path);
	}
}
/* 
 * DESC:
 * function run external PERL script using fork function
 *
 * in:
 * path to PERL script
 * out:
 * parent - no return value 
 * child - shouldn't back
 */
void run_prog(char *prog1) 
{
	char str[1024];
	int good = 0;
	char *prog = prog1;
	char scripts[PATH_MAX];

	sprintf(str, "<%s/>", prog);
	WLOG_NB("run_prog: exec %s\n", prog);

	if (! strncmp(prog, "script:", 7)) {
		int l;
		time_t t = time(0);
		sprintf(str, "<%s/><atime:%ld/>\n", prog, t);
		l = strlen(str);
		if (pipe_fd < 0) open_pipe();
		if (pipe_fd >= 0) {
			int a = write (pipe_fd, str, l);
			WLOG_NB("run_prog: write %d chars to pipe strlen is %d atime %ld\n", a, l, t);
			if (a < 1) {
				close (pipe_fd);
				pipe_fd = -1;
			} else {
				good = 1;
			}
		}
		if (! good) {
			sprintf(scripts, "%s/%s.pl %s", scripts_dir, &prog[7], ids_name);
			WLOG("run_prog: run normal: %s\n", scripts);
                        prog = scripts;
		}
	}	
	if (! good)  {
		char perl[]="/usr/bin/perl";
		int a;
		int pid = fork();
	        uint16_t l = strlen(prog);
       		char *exec = prog;
        	char *args = 0;

		 int lid, eid;
	        char log_file[PATH_MAX];
        	char err_file[PATH_MAX];
		/*
	         * is fork return error ?
	         */
	        if (pid == -1) { 
	                WLOG("run_prog: error to run job: %s\n", strerror(errno));
	                return;
	        }
	        /*
	         * No, so we are parent, so return ASAP from this
	         */
	        if (pid) {
	                WLOG_NB("created %d for %s\n", pid,prog);
	                return;
	        }
	        /*
	         * Do child work
	         */
	        /*
	         * divide path with args
	         */
	        
	        sprintf(log_file,"%s/", log_dir);
	
	        for (a = 0; a < l; a++) {
	                if (prog[a] < 33) {
	                        prog[a] = 0;
	                        args = &prog[a + 1]; 
	                        break;
	                }
	        }
	        /*
	         * find full pwd and prog name
	         */
	        for (a = strlen(exec);a > 0;a--) 
	                if (exec[a] == '/') break;
	        /*
	         * create log and err file names
	         */
	        sprintf(log_file,"%s/%s.log", log_dir, &exec[a + 1]);
	        sprintf(err_file, "%s/%s.err", err_dir, &exec[a + 1]);
		
		if ( (! strncmp(&exec[a + 1], "ping", 4)) || (! strncmp(&exec[a + 1], "troute", 6)) ) {
			/*
			 * still as r00t
			 */
			WLOG_NB("run_prog: still as r00t because of: %s\n", &exec[a +1]);
		} else {
			/*
			 * switch to dedicated user
			 */
			if (setgid(wuid)) {
	                        WLOG("run_prog: error to setgid %s\n", strerror(errno));
	                }
	                if (setuid(wuid)) {
	                        WLOG("run_prog: error to setuid %s\n", strerror(errno));
	                }
			WLOG_NB("run_prog: switch to user becuase of: %s\n", &exec[a + 1]);
		}
		/*
	         * redirect stdout and stderr to dedicated files
	         */
	        lid = open(log_file, O_WRONLY| O_APPEND | O_CREAT,0644);
	        if (lid < 0)  {
	                WLOG("run_prog: error in open %s file: %s\n", log_file, strerror(errno));
	        } else 
	                dup2(lid, 1);
	        
	        eid = open(err_file,  O_WRONLY| O_APPEND|  O_CREAT,0644);
	        if (lid < 0)  {
	                WLOG("run_prog: error in open %s file: %s\n", err_file, strerror(errno));
	        } else 
	                dup2(eid, 2);

		a = strlen(exec);
	        if ((exec[a - 3] == '.') && (exec[a - 2] == 'p') && (exec[a - 1] == 'l')) 
                /*
                * EXEC perl with script name
                */
                	execl(perl,perl, exec,args, NULL);
        	else 
			execl(exec, exec, args,NULL);

	        /*
	         * on success shouldn't back here
	         */
	        WLOG("run_prog: exec error %s\n", strerror(errno));
	        /*
	         * exit as we are child
	         */
	        exit(0);
	}	
}
void main_child() {
	int status;
	pid_t p = wait(&status);
	status <<= 8;
	WLOG("main_child: pid %d end with code %d\n", p, status);
	return;
}

/*
 * DESC:
 * count string len but any white space is delimeter
 *
 * IN:
 * str - ptr to string
 * OUT:
 * len of string
 */
int strlen_w(char *name)
{
	int l;
	for(l = 0;l < 4096;l++)
		if (name[l] < 33) return l;
	return 0;
}
/*
 * DESC:
 * Szukanie w str ciagu z str1, ograniczniem jest bialy znak (rowniez spacja)
 * IN:
 * str - ciag znakow, w ktorych mam szukac
 * str1 - ciag znakow, ktory szukamy
 * OUT:
 * 0 - nie znaleziono
 * >0 - index, w ktorym str znalezlismy pierwsza litere z str1
 *
 */
int32_t strfind(char *str, char *str1) {
    int size = strlen_w(str);
    int size1 = strlen_w(str1);
    int a, b = 0;
    int c = -1;
	#ifdef STRFIND
	if (debug) WLOG("str1 %s (size %d) str2 %s (size %d)\n", str, size, str1, size1);
	#endif
    for (a = 0; a < size; a++) {
        if (str[a] == str1[b]) {
            if (b == 0) c = a;
            b++;
            if (b == size1) 
                return c;
        } else {
            if (b) a--;
            b = 0;
            c = -1;
        }
    }
    return -1;
}
/*
 * DESC:
 * add task to particual (l) table
 * l table can be second, minutes, days, month, day o fweek
 * IN:
 * ptr - ptr to table (sek,min,mon,wday)
 * n - task number
 * OUT:
 * 0 - no success
 * ptr -  to new line
 */
int16_t *add_n_i (int16_t *l, int16_t n)
{
	 int16_t *o = 0;
	int16_t  x = 0;
	/* 
	 * check is this first line
	 */
	if (! l) {
		#ifdef ADD_N_I
		 if (debug) WLOG("create for %d\n", n);
		#endif
		/*
		 * alloc first memory and set defaults on begin 
		 * first argument is len of this table
		 */
		o = calloc(2,sizeof(int16_t));
		if (! o) {
			if (debug) WLOG("add_n_i: error to malloc memory: %s\n", strerror(errno));
			return 0;
		}
		o[0] = 1;
		o[1] = n;
	/*
	 * add to lines
	 */
	} else {
	/*
	 * get members count from first argument
	 */
		x = l[0] + 1;
		 #ifdef ADD_N_I
                 if (debug) WLOG("realloc dla zadania %d do x %d\n", n, x);
                #endif
		/*
		 * x + 1 as we need to alloc from len itself too
		 */
		o = realloc (l, (x + 1) * sizeof(int16_t));
		if (! o) {
                	if (debug) WLOG("add_n_i: error to realloc memory: %s\n", strerror(errno));
                        return 0;
                }

		/*
		 * put on end
		 */
		o[x] = n;
		/*
		 * put new len into first table element
		*/
 		o[0] = x;
	}
	/*
	 * return to new array, in general same as input as realloc try to use same pointer
	 */
	return o;
}
/*
 * OUT:
 * 0 - wrong line
 * 1 - ok
 */

int add_n (int16_t **c, char *line, int len, int16_t task)
{ 
	int16_t i,s = strlen_w(line);
	/*
	 * for * fill all arrays with this task
	 */
	if (line[0] == '*') {
		int16_t l = 0;

		for (l = 0; l <= len; l++) 
			c[l] = add_n_i(c[l], task);
		
		return 1;
	}
	for(i = 0;i < s;i++) {
		/*
		 * liczba - what string we found
		 */
		int16_t l;
		/*
		 * check is this digit
		 */
		if ((line[i] < '0') || (line[i] > '9')) return 0;
		/*
		 * get this digit
		 */
		l = atol(&line[i]);
		#ifdef ADD_N
		 if (debug) WLOG("add_n: l %d c %p ptr %p\n", l, c, c[l]); 
		#endif
		/*
		 * TODO: check return value
		 */
		c[l] = add_n_i(c[l], task);
			
		#ifdef ADD_N
		if (debug) WLOG("add_n: return ptr %p for %d numbers %d\n", c[l], l,c[l][0]);
		#endif
		
		/*
		 * jump into next digit (search , and then jump to next char)
		 */
		 for(;i < s;i++)  
			if (line[i] == 44) break;
	}
	return 1;
}

#define NEXT_PARAM for (j = 0,i = 0; ;i++) { if (line[i] < 33) j = 1; if ((j) && (line[i] > 32)) {line = &line[i]; break;} }

/*
 * function load and parse data for file
 * global var in use:
 * sec - fill this array
 * cmd - fill this array
 * in:
 * name - file name to parse
 * out:
 * 0 - mean error
 */
int16_t load_cfg_file (char *base, int16_t task)
{
	int f;
	struct stat file;
        char *buf;
	char name[PATH_MAX];

	uint32_t buf_size, z = 0, i, j;
	char *line;
	get_cfg_filename(base,name);
        f = open(name, O_RDONLY);
	if (f < 0) { WLOG("unable to find cfg file %s\n", name); return 0; }
        fstat (f, &file);
	buf_size = file.st_size;
        buf = malloc(buf_size + 1);
	memset(buf, 0, buf_size + 1);

	if (! buf) {
		 if (debug) WLOG("unable to malloc memory\n"); 
		return 0;
	} 
	/*
	 * load file into big buffer
	 */
	for (i = 0; i < buf_size; i += 4096) {
        	if (read(f, &buf[i], 4096) < 4096) break;
	}
	/*
	 * now parse it
	 */
	line = buf;
	// task = 0;
again:
         for (;z < buf_size;z++) {
         	char c = buf[z];
                if (c == 0) break;
                /*
                 * change EOL to \0 for strings commands
                 */
                if (c == '\n') {
                	buf[z] = 0;
                        break;
                }
         }
         /*
          * work on line from cfg
          */
         // start with '#' ommit
         if (line[0] == '#') goto again_next;

	if (strfind(line, "LOGDIR=") == 0) {
		memcpy(log_dir, &line[7], strlen(&line[7]));
		printf("LOG_DIR %s\n", &line[7]);
	}
	if (strfind(line, "ERRDIR=") == 0) {
                memcpy(err_dir, &line[7], strlen(&line[7]));
                printf("ERR_DIR %s\n", &line[7]);
        }
/*	if (strfind(line, "MAIN=") == 0) {
                memcpy(perl_main, &line[5], strlen(&line[5]));
                printf("Perl main:%s\n", &line[5]);
        }*/

	 if (strfind(line, "PIDDIR=") == 0) {
                memcpy(pid_file, &line[7], strlen(&line[7]));
                printf("PID_DIR %s\n", &line[7]);
        }
	if (strfind(line, "SCRIPTS=") == 0) {
                memcpy(scripts_dir, &line[8], strlen(&line[8]));
                printf("SCRIPTS dir %s\n", &line[8]);
        }
	if (strfind(line, "USER=") == 0) {
		struct passwd *pass;
		pass = getpwnam(&line[5]);
		if (pass) {
			wuid = pass->pw_uid;
			wgid = pass->pw_gid;

			sprintf(pipe_path, "%s-%s", pipe_path, pass->pw_name);
			printf("User: %s with UID: %d and GID %d\n", pass->pw_name, wuid, wgid);
			sprintf(w_user, "%s", pass->pw_name);
		} else {
			printf("User not found in passwd file\n");
		}
        }
	
	 if (strfind(line, "GID=") == 0) {
                wgid = atol (&line[4]);
                printf("GID %d\n", wgid);
        }

	/*
	 * check server name in first column
	 * if not found * or hostname then jump into next line
	 */
	if ((line[0] != '*') && (strfind(line, ids_name)) == -1) {goto again_next; }
	/*
	 * omit hostname, jump into next values
	 */
	NEXT_PARAM;
	/*
	 * parse seconds
	 */
	 if (debug) WLOG("Sek Line: %s %p\n", line, sec);

	add_n(sec,line, 59, task);
	/*
	 * move line to minutes
	 */
	NEXT_PARAM;
	 if (debug) WLOG("Min Line: %s\n", line);
	add_n(min,line, 59, task);
	/*
	 * move to hour
	 */
	NEXT_PARAM;
         if (debug) WLOG("Hour Line: %s\n", line);
        add_n(hour,line, 23, task);
	/*
	 * move to day
	 */
	NEXT_PARAM;
         if (debug) WLOG("Day Line: %s\n", line);
        add_n(day,line, 31, task);
	/*
         * move to day in week
         */
	NEXT_PARAM;
         if (debug) WLOG("Day of Week Line: %s\n", line);
        add_n(wday,line, 7, task);
	NEXT_PARAM;
	 if (debug) WLOG("Time zone: %s\n", line);
	if (line[0] == '0') 
		tz[0] = add_n_i(tz[0], task);
	else if (line[1] == '1') 
		tz[1] = add_n_i(tz[1], task);
	else 
		 if (debug) WLOG("unkown tz: %d\n", line[0]);
	/*
	 *  move to task
	 */
	NEXT_PARAM;

	cmd = realloc(cmd, (task + 1) * sizeof(char *));
	j = strlen(line);
	if (debug) WLOG("Rest: j %d %s\n",j, line);
	cmd[task] = malloc(j + 1);
	// memset(cmd[task], 0, j + 1);
	memcpy (cmd[task], line, j);
	 cmd[task][j] = 0;
	if (debug) WLOG("Task %d: %s\n", task, cmd[task]);

	task++;
again_next:
         /*
          * jump to next string
          */
         if (++z < buf_size) {
         	line = &buf[z];
                goto again;
         }

	return task;
}
/*
 * function load cron table from IDS system
 * in:
 * out:
 * n - total number of loaded task
 *
 */
int16_t load_cfg(char *base) 
{
	int n;
	int16_t task = 1;
	for (n= 0;n < 60;n++) {
		sec[n] = 0;
		min[n] = 0;
	}
	return load_cfg_file(base, task);
}	
#define VIEW_LINE(xx) for (i = 0; i < max_task; i++) {  if ((debug) && (xx[i])) WLOG("task %d/%d task %d\n", i, max_task, xx[i]); }

void main_loop(uint16_t *, uint16_t);

int main(int argc, char *argv[])
{
	/*
	* check if pid file exists
	*/
	uint16_t max_task = 0;
	int f,i;
	uint16_t *task;
	pid_t pid;
	/*
	 * get hostname from args (prefered) or from system
	 */
	if (argc > 1) {
		memcpy(ids_name, argv[1], strlen(argv[1]));
 	} else {
		gethostname(ids_name, HOST_NAME_MAX);
	}
	
	/*
	 * load cfg
	 */
	max_task = load_cfg(argv[0]);
	if (debug) WLOG("hostname: %s\n", ids_name);

	if (debug) WLOG("loaded: %d tasks\n", max_task - 1);

	task = calloc(max_task, sizeof(int16_t));
	if (!task) {
		 if (debug) WLOG("unable to alloc memory for tasks\n");
		return 0;
	}	
	/*
	 * deamonize for launcher
	 */
	pid = fork();

	if (pid < 0) {
        	printf ( "Can't fork\n");
        	exit(EXIT_FAILURE);
	}
	/*
	 * CHILD
	 */
	if (pid == 0) {
        	pid_t sid = setsid();

        	if (sid < 0)
            		exit(EXIT_FAILURE);
		/* 
		 * close unused FID
		 */

       		close(STDIN_FILENO);
        	close(STDOUT_FILENO);
        	close(STDERR_FILENO); 
        	/*
		* SIGNALS
        	* Control - C save exit
        	*/
        /*signal(SIGINT, (void *)finish);
        signal(SIGUSR1, (void *)signal_usr1);
        signal(SIGHUP, (void *)reload_cfg);*/

		 signal(SIGCHLD, SIG_IGN);
		signal(SIGPIPE, SIG_IGN);
		close (fidlog);
		fidlog = -1;
        	main_loop(task, max_task);
        	exit (0);
	}
	
        
	/*
	 * write pid into file
	 */
	 i = strlen(argv[0]);
	for (;i > 0;i--)
            if (argv[0][i] == '/') break;

        sprintf(pid_file,"%s/%s.pid", pid_file, &argv[0][i]);

	f = creat(pid_file, 0775);
        if (f > -1) {
            char n[20];
            sprintf(n, "%d", pid);
            if (write(f, n, strlen(n)) == -1) {
                WLOG("main: unable to write pid file in %s error: %s\n", pid_file, strerror(errno));
            } else {
                WLOG( "main: file %s with pid save succesfully\n", pid_file);
            }
            close (f);
        } else  {
            WLOG("main: unable to create pid file %s error: %s\n", pid_file, strerror(errno));
     }

    if (debug) printf("main: create child with pid %d\n", pid);

    return (EXIT_SUCCESS);

}
/*
 * DESC:
 * main loop
 * OUT:
 * should never end :)
 */
void main_loop(uint16_t *task, uint16_t max_task) 
{
	time_t t = time(0);
	/*
	 * get first time
	 * then we only base in clock_nanosleep call
	 */
	for (;;) {
		int8_t ti;
		/*
		 * run for two zones, GMT (for tasks) and local zones (for reports)
		 */
		for (ti = 0; ti < 1; ti++) {
			struct tm *czas;
			if (ti == 0) czas = gmtime(&t);
			if (ti == 1) czas = localtime(&t);
			memset(task, 0, max_task);
			/*
			 * check is we have any task 
			 */
			if ((sec[czas->tm_sec]) && (min[czas->tm_min]) && (hour[czas->tm_hour]) && (day[czas->tm_mday]) && (wday[czas->tm_wday])) {
				uint16_t i,j, ok;
				/*
				 * first column is number of tasks
				 */
				uint16_t tasks = sec[czas->tm_sec][0];
				/*
				 * can't use any WLOG as we use localhost data to speed up this process
				 */
				// if (debug) WLOG("nr of tasks: sec %d min %d hour %d day %d wday %d\n", sec[czas->tm_sec][0], min[czas->tm_min][0], hour[czas->tm_hour][0], day[czas->tm_mday][0], wday[czas->tm_wday][0]);
				/*
				 * copy seconds tasks to tasks
				 */
				for (i = 0; i < tasks; i++) {
					task[i] = sec[czas->tm_sec][i + 1];
				}
				/*
				 * delete from tasks which are not fit with minutes
				*/
				for (i = 0; i < tasks; i++) {
					ok = 0;
					if (task[i]) {
						for(j = 0;j < min[czas->tm_min][0];j++) {
							if (task[i] == min[czas->tm_min][j + 1]) { ok = 1; break; }
						}
						if (! ok) task[i] = 0;
					}
				}

				for (i = 0; i < tasks; i++) {
					ok = 0;
                                        if (task[i]) {
                                                for(j = 0;j < hour[czas->tm_hour][0];j++) {
                                                        if (task[i] == hour[czas->tm_hour][j + 1]) { ok = 1; break; }
                                                }
                                                if (! ok) task[i] = 0;
                                        }
                                }
				
                                for (i = 0; i < tasks; i++) {
					ok = 0;
                                        if (task[i]) {
                                                for(j = 0;j < day[czas->tm_mday][0];j++) {
                                                        if (task[i] == day[czas->tm_mday][j + 1]) { ok = 1; break; }
                                                }
                                                if (! ok) task[i] = 0;
                                        }
                                }
                                for (i = 0; i < tasks; i++) {
					ok = 0;
                                        if (task[i]) {
                                                for(j = 0;j < wday[czas->tm_wday][0];j++) {
                                                        if (task[i] == wday[czas->tm_wday][j + 1]) { ok = 1; break; }
                                                }
                                                if (! ok) task[i] = 0;
                                        }
                                }
				  if (debug) WLOG_NB("Current time: %04d-%02d-%02d %02d:%02d:%02d\n", czas->tm_year + 1900, czas->tm_mon + 1, czas->tm_mday, czas->tm_hour, czas->tm_min, czas->tm_sec);
				/*
				 * run all remain tasks
				 */
				for (i = 0; i < max_task; i++) {  
					if (task[i]) {
						j = task[i];
						WLOG_NB("task %d/%d task %d\n", i, max_task, j); 
						
						run_prog(cmd[j]);
						task[i] = 0;
					}
						
				}

			} 
		}
		
		{
			struct timespec wa;
			/*
	                 * wait to start second
       	        	 */
                	wa.tv_sec = ++t;
                	wa.tv_nsec = 0;
//			WLOG("wating till %u\n", (uint32_t) wa.tv_sec);
                	if (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &wa, NULL)) {
                        	if (debug) WLOG("MAIN: nanosleep with error %s again looping\n", strerror(errno));
			}
                }
			
//		if (debug) WLOG("GET time %d\n", (int)t);

	}
	return;
}
