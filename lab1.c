#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

int maxSignal = 0;
int maxExit;
int exitStatus = 0;
int fdCount = 0;
int pidCount = 0;
int c = -1;
int verboseFlag = 0;
int* fdArr;
int* pidArr;
char*** argArray;
int currentOption = 0;
int openFlags = 0;
int profileFlag = 0;
struct rusage usage_start;
struct rusage usage_end;

struct options {
    char opt;
    char* arg;
};

int startsWith(const char *str1, const char *str2)
{
  if(strncmp(str1, str2, strlen(str2)) == 0)
    return 1;
  else
    return 0;
}

void createFileDescriptor(char* file, int fileOption, const char* optionName) {
  int fd = open(file, fileOption | openFlags, 0644);
  openFlags = 0;
  fdCount++;
  fdArr = realloc(fdArr, fdCount*sizeof(int));
  fdArr[fdCount-1] = fd;
  if (fd < 0) {
      fprintf(stderr, "--%s unable to open file %s: %s\n", optionName, file, strerror(errno));
      exitStatus = 1;
  }
}

void createPipe() {
  int p[2];
  int ret = pipe(p);
  fdCount = fdCount + 2;
  fdArr = realloc(fdArr, fdCount*sizeof(int));
  fdArr[fdCount-2] = p[0];
  fdArr[fdCount-1] = p[1];
  if (ret < 0) {
      fprintf(stderr, "--pipe unable to create pipe: %s\n", strerror(errno));
      exitStatus = 1;
  }
}

void runCommand(char** arguments) {
  int pid = fork();
  if(pid == 0) {
    int fd1 = fdArr[atoi(arguments[0])];
    int fd2 = fdArr[atoi(arguments[1])];
    int fd3 = fdArr[atoi(arguments[2])];
    //childProcess
    dup2(fd1,0);
    dup2(fd2,1);
    dup2(fd3,2);

    for(int i = 0; i < fdCount; i++) {
      close(fdArr[i]);
    }

    int error = execvp(arguments[3], arguments+3);
    if(error == -1) {
      fprintf(stderr, "Error Occurred while runnning %s: %s\n", arguments[3], strerror(errno));
      exitStatus = 1;
    }
  }
  else if(pid < 0){
    fprintf(stderr, "Error running --command\n");
    exitStatus = 1;
  }
  else {
    pidCount++;
    pidArr = realloc(pidArr, pidCount*sizeof(int));
    argArray = realloc(argArray, pidCount*sizeof(char**));
    pidArr[pidCount-1] = pid;
    argArray[pidCount-1] = arguments + 3;
  }
}

void closeFD(int fd){
  int ret = close(fdArr[fd]);
  if(ret < 0){
    fprintf(stderr, "Unable to close file descriptor %d\n", fd);
    exitStatus = 1;
  }
  fdArr[fd] = -1;
}

char** parseArgs(int argc, char** argv, int *optind) {
  char** arguments = malloc(0);
  int argCounter = 0;
  for((*optind)-- ; *optind < argc && startsWith(argv[(*optind)],"--") != 1; (*optind)++){
    argCounter++;
    arguments = realloc(arguments, argCounter*sizeof(char*));
    if(argCounter <= 3 && atoi(argv[(*optind)]) > fdCount-1) {
      fprintf(stderr, "File Descriptor entered must be less than %d\n", fdCount);
      exitStatus = 1;
    }
    else
      arguments[argCounter-1] = argv[(*optind)];
  }
  arguments = realloc(arguments, (argCounter+1)*sizeof(char*));
  arguments[argCounter] = NULL;
  if(verboseFlag) {
    printf("--command");
    for(int i = 0; i < argCounter; i++){
      printf(" %s", arguments[i]);
    }
    printf("\n");
    fflush(stdout);
  }

  if(argCounter < 4) {
    fprintf(stderr, "--command must have at least 4 arguments\n");
    exitStatus = 1;
  }

  if(fdArr[atoi(arguments[0])] < 0 || fdArr[atoi(arguments[1])] < 0 || fdArr[atoi(arguments[2])] < 0) {
    fprintf(stderr, "file descriptor closed\n");
    exitStatus = 1;
  }

  return arguments;
}

void sigHandler(int signum) {
  fprintf(stderr, "%d caught\n", signum);
  exit(signum);
}

void waitForChildren() {
  for(int i = 0; i < pidCount; i++) {
    int wstatus;
    int pid = wait(&wstatus);
    int currCommand;
    for(int j = 0; j < pidCount; j++){
      if(pidArr[j] == pid)
        currCommand = j;
    }
    if(WIFEXITED(wstatus)){
      printf("exit %d ", WEXITSTATUS(wstatus));
      if(WEXITSTATUS(wstatus) > maxExit)
        maxExit = WEXITSTATUS(wstatus);
    }
    else if(WIFSIGNALED(wstatus)){
      printf("signal %d ", WTERMSIG(wstatus));
      if(WTERMSIG(wstatus) > maxSignal)
        maxSignal = WTERMSIG(wstatus);
    }
    for(int j = 0; argArray[currCommand][j] != NULL; j++) {
      printf("%s ", argArray[currCommand][j]);
    }
    printf("\n");
    fflush(stdout);
  }
  for(int i = 0; i < pidCount; i++) {
    free(argArray[i] - 3);
  }
  free(argArray);
  free(pidArr);
  argArray = malloc(sizeof(char**));
  pidArr = malloc(sizeof(int));
  if(profileFlag){
      getrusage(RUSAGE_CHILDREN, &usage_end);
      double userTimeDiff = (double) usage_end.ru_utime.tv_sec + usage_end.ru_utime.tv_usec/1000000.;
      double sysTimeDiff = (double) usage_end.ru_stime.tv_sec + usage_end.ru_stime.tv_usec/1000000.;
      double total = userTimeDiff + sysTimeDiff;
      printf("children   user: %f sys: %f total: %f\n", userTimeDiff, sysTimeDiff, total);
      fflush(stdout);
  }
}

int main(int argc, char **argv)
{
    static struct option long_options[] =
    {
        {"rdonly", required_argument, 0, 1 },
        {"wronly", required_argument, 0, 2},
        {"command", required_argument, 0, 3},
        {"verbose", no_argument, 0, 4 },
        {"append", no_argument, 0, 5},
        {"cloexec", no_argument, 0, 6},
        {"creat", no_argument, 0, 7},
        {"directory", no_argument, 0, 8},
        {"dsync", no_argument, 0, 9},
        {"excl", no_argument, 0, 10},
        {"nofollow", no_argument, 0, 11},
        {"nonblock", no_argument, 0, 12},
        {"rsync", no_argument, 0, 13},
        {"sync", no_argument, 0, 14},
        {"trunc", no_argument, 0, 15},
        {"rdwr", required_argument, 0, 16},
        {"pipe", no_argument, 0, 17},
        {"wait", no_argument, 0, 18},
        {"close", required_argument, 0, 19},
        {"abort", no_argument, 0, 20},
        {"catch", required_argument, 0, 21},
        {"ignore", required_argument, 0, 22},
        {"default", required_argument, 0, 23},
        {"pause", no_argument, 0, 24},
        {"profile", no_argument, 0, 25},
        {0, 0, 0, 0}
    };
    fdArr = malloc(sizeof(int));
    pidArr = malloc(sizeof(int));
    argArray = malloc(sizeof(char**));
    while((c = getopt_long(argc, argv,"", long_options, &currentOption)) != -1) {
        if(profileFlag)
          getrusage(RUSAGE_SELF, &usage_start);
        switch(c)
        {
            case 1:
                if(verboseFlag) {
                  printf("--rdonly %s\n", optarg);
                  fflush(stdout);
                }
                createFileDescriptor(optarg, O_RDONLY, long_options[currentOption].name);
                break;
            case 2:
                if(verboseFlag) {
                  printf("--wronly %s\n", optarg);
                  fflush(stdout);
                }
                createFileDescriptor(optarg, O_WRONLY, long_options[currentOption].name);
                break;
            case 3:
                ;
                char** arguments = parseArgs(argc, argv, &optind);
                runCommand(arguments);
                //free(arguments);
                break;
            case 4:
                verboseFlag = 1;
                break;
            case 5:
                if(verboseFlag) {
                  printf("--append\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_APPEND;
                break;
            case 6:
                if(verboseFlag) {
                  printf("--cloexec\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_CLOEXEC;
                break;
            case 7:
                if(verboseFlag) {
                  printf("--creat\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_CREAT;
                break;
            case 8:
                if(verboseFlag) {
                  printf("--directory\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_DIRECTORY;
                break;
            case 9:
                if(verboseFlag) {
                  printf("--dsync\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_DSYNC;
                break;
            case 10:
                if(verboseFlag) {
                  printf("--excl\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_EXCL;
                break;
            case 11:
                if(verboseFlag) {
                  printf("--nofollow\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_NOFOLLOW;
                break;
            case 12:
                if(verboseFlag) {
                  printf("--nonblock\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_NONBLOCK;
                break;
            case 13:
                if(verboseFlag) {
                  printf("--rsync\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_RSYNC;
                break;
            case 14:
                if(verboseFlag) {
                  printf("--sync\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_SYNC;
                break;
            case 15:
                if(verboseFlag) {
                  printf("--trunc\n");
                  fflush(stdout);
                }
                openFlags = openFlags | O_TRUNC;
                break;
            case 16:
                if(verboseFlag) {
                  printf("--rdwr %s\n", optarg);
                  fflush(stdout);
                }
                createFileDescriptor(optarg, O_RDWR, long_options[currentOption].name);
                break;
            case 17:
                if(verboseFlag) {
                  printf("--pipe\n");
                  fflush(stdout);
                }
                createPipe();
                break;
            case 18:
                if(verboseFlag) {
                  printf("--wait\n");
                  fflush(stdout);
                }
                waitForChildren();
                pidCount = 0;
                break;
            case 19:
                if(verboseFlag) {
                    printf("--close %s\n", optarg);
                    fflush(stdout);
                }
                closeFD(atoi(optarg));
                break;
            case 20:
                if(verboseFlag) {
                    printf("--abort\n");
                    fflush(stdout);
                }
                raise(SIGSEGV);
                break;
            case 21:
                if(verboseFlag) {
                    printf("--catch %s\n", optarg);
                    fflush(stdout);
                }
                signal(atoi(optarg), sigHandler);
                break;
            case 22:
                if(verboseFlag) {
                    printf("--ignore %s\n", optarg);
                    fflush(stdout);
                }
                signal(atoi(optarg), SIG_IGN);
                break;
            case 23:
                if(verboseFlag) {
                  printf("--default %s\n", optarg);
                  fflush(stdout);
                }
                signal(atoi(optarg), SIG_DFL);
                break;
            case 24:
                if(verboseFlag) {
                    printf("--pause\n");
                    fflush(stdout);
                }
                pause();
                break;
            case 25:
                if(verboseFlag) {
                  printf("--profile\n");
                  fflush(stdout);
                }
                profileFlag = 1;
                continue;
                break;
            default:
                fprintf(stderr, "Invalid input\n");
                exitStatus = 1;
                break;
        }
        if(profileFlag){
            getrusage(RUSAGE_SELF, &usage_end);
                //fprintf(stderr, "Error getting times");
            double userMicroSecDiff = difftime(usage_end.ru_utime.tv_usec, usage_start.ru_utime.tv_usec);
            double userSecDiff = difftime(usage_end.ru_utime.tv_sec, usage_start.ru_utime.tv_sec);
            double sysMicroSecDiff = difftime(usage_end.ru_stime.tv_usec, usage_start.ru_stime.tv_usec);
            double sysSecDiff = difftime(usage_end.ru_stime.tv_sec, usage_start.ru_stime.tv_sec);
            double userTimeDiff = userSecDiff + userMicroSecDiff/1000000.0;
            double sysTimeDiff = sysSecDiff + sysMicroSecDiff/1000000.0;
            double total = userTimeDiff + sysTimeDiff;
            printf("--%s   user: %f sys: %f total: %f\n", long_options[currentOption].name , userTimeDiff, sysTimeDiff, total);
            fflush(stdout);
        }


    }
    free(fdArr);

    if(maxSignal){
      signal(maxSignal, SIG_DFL);
      raise(maxSignal);
    }
    else if(maxExit)
      exit(maxExit);
    else
      exit(exitStatus);
}
