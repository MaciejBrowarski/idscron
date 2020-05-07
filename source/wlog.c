/*
 * WLOG function
 * common use by all programs
 */
#include <sys/time.h>
#define PREFIX_LOG "idscron"

// #define WLOG(str...) {  pthread_mutex_lock(&wlog_lock);  sprintf(log_buf, ## str);   wlog(log_buf, 1);  pthread_mutex_unlock(&wlog_lock); }
#define WLOG(str...) {    sprintf(log_buf, ## str);   wlog(log_buf, 1);  }
#define WLOG_NB(str...) {    sprintf(log_buf, ## str);   wlog(log_buf, 0);  }


char log_dir[PATH_MAX] = "/var/tmp";
char log_buf[4096];

int16_t fidlog = -1;
extern uint8_t debug;
/*
 *
 * function name: wlog
 * synopsis:
 * write log to file pointed by FID
 * FID is assign when wlog is running first time
 * argument
 *   in:
 *      1 - string
 *   out:
 *
 */
int wlog(char *string, uint8_t flush)
{
    int str;
    int ret;
    struct tm *czas;
    struct timeval cz;
    char buf[4096];
    char fname[100];
    /*
     * get current time with milisecunds
     */
    gettimeofday(&cz, NULL);

    czas = localtime(&cz.tv_sec);
    /*
     * if file handler for log is -1
     * create file with log
     */
    if (fidlog == -1) {
        memset(fname, 0, 100);
#ifdef WIN32
      uint32_t pid = 1000;
	/*
	 * folder/PREFIX-PID.timestamp
	 */
        sprintf(fname,"%s\\%s-%d.%d", log_dir, PREFIX_LOG, pid, (uint32_t) time(0));
        fidlog = open(fname, O_WRONLY|O_CREAT|O_TRUNC,0755);
#else
        uint32_t pid = getpid();
        sprintf(fname,"%s/%s-%d.%d", log_dir, PREFIX_LOG, pid, (uint32_t) time(0));
        fidlog = open(fname, O_WRONLY|O_CREAT|O_TRUNC,0755);

#endif
	if (fidlog < 0) {
            perror("Unable to create log file ");
            exit(1);
      
        }
    }
    /*
     * add to log string information about time
     */
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d.%06u %s", czas->tm_year + 1900,czas->tm_mon + 1, czas->tm_mday, czas->tm_hour, czas->tm_min, czas->tm_sec, (uint32_t) cz.tv_usec, string);
    str = strlen(buf);
    /*
     * write to file
     */
    ret = write (fidlog, buf, str);
	if (flush) fdatasync(fidlog);

	if (debug > 1) write (1, buf, str);
    /*
     * no exit when error to write, maybe some other method to inform
     * user that log can't write
     * exit isn't good idea
     */
    if (ret != str) { }
    //    exit(1);

    return 0;
}

/*
 * generated, based on argv[0] (which should be provided by ptr *a)
 * configuration file
 *
 * used by:
 * filec -> cfg/filec.cfg
 * idscron -> cfg/idscron.cfg
 *
 * out:
 * 0 - good
 * 1- bad
 */
int get_cfg_filename(char *base, char *cfg) 
{
        /*
         * is base begin as full path
         */
         if (base[0] == '/') {
                char *a;
                // add cfg on end
                sprintf(cfg,"%s.cfg", base);
                // rename bin with cfg
                a = strstr(cfg, "bin");
                if (a) {
                        a[0] = 'c';
                        a[1] = 'f';
                        a[2] = 'g';
                        return 0;
                }
        } else {
        /*
         * exeute without absolute path, use default location
         */
                int a;
                for(a =  strlen(base); a> 0; a--)
                        if (base[a] == '/') {  break; }
                a++;
                if (! strncmp (&base[a], "filec", 5)) {
                        sprintf (cfg, "%s/get/netbone/cfg/filec.cfg", getenv("HOME"));
                        return 0;
                }
                if (! strncmp (&base[a], "idscron", 7)) {
                        sprintf (cfg, "%s/get/idscron/cfg/idscron.cfg", getenv("HOME"));
                        return 0;
                }       
        }
	return 1;
}
