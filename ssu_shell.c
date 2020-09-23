#include  <stdio.h>
#include  <sys/types.h>
#include <stdlib.h>
#include<sys/resource.h>
#include<sys/wait.h>
#include<time.h>
#include <string.h>
#include <unistd.h>
#include<getopt.h>
#include <sys/sysctl.h>
#include<utmp.h>
#include<dirent.h>
#include<string.h>
#include<ctype.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<pwd.h>
#include<ncurses.h>
#include<signal.h>
#include<linux/kdev_t.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 32
#define MAX_NUM_TOKENS 64
#define BUFSIZE 512
#define HZ 100 //getconf CLK_TCK (mine is 100)

static char **pps_argv;
char* ttopgraph[BUFSIZE][13]={{0}};
enum ttopgraphh{PID, GUSER, PR, NI, VIRT, RES, SHR, S, CPU, MEM, TIME, COMMAND};
char* ppsinfo[BUFSIZE][20]={{0}};
char* tty[BUFSIZE][5]={{0}};
	int tty_maj[BUFSIZE];
	int tty_min[BUFSIZE];
enum ppsinfo{_PID, _USER, _TTY, _STAT, _TIME, _COMMAND, _CPU, _MEM, _VSZ, _RSS, _START};
	long uptime;//get_uptime()
int totalram;//get_mem()
int totaltasks;//get_procnum()
int totalcpuusage;//get_cpu()


static void jiffies_to_timespec(unsigned long jiffies, struct timespec *value);
static int totprocess;

volatile sig_atomic_t print_flag=false;
void handle_alarm(int sig);
static void redirect(int oldfd, int newfd);

int ppsopt_parse (char **buf);
void get_ttyname();
void get_username();
enum ppsopt{UOPT, XOPT, UXOPT };
void ppsprint_aux();
void ppsprint_none(char *ttyname);
void ppsprint_ax();
void ppsprint_au();
void ppsprint_a();
void ppsprint_ux(char *username);
void ppsprint_u(char *username);
void ppsprint_x(char *username);
void run_pps();
void get_psproc(int psopt);
int get_cputime(ulong utime, ulong stime, ulong starttime, int seconds);

static time_t seconds_since_1970;

void open_task(int k, int pidnum, char* procstate,int plusflag);
//void open_task();
void open_status(int k, int session, char* procstate, char* cmdline);
void get_psuser(char* statfile, int k);
void get_pstty(int tty_maj,int tty_min, int k);
void run_ttop();
void repeat_ttop();
void get_graph();
int get_uptime(int ttopprint);
void get_user();
void get_loadav();
void get_procnum(int graphflag);
void get_cpu();
void get_mem(int ttopflag);
//static void print_uptime(size_t n, const STRUCT_UTMP *this);

/* Splits the string by space and returns the array of tokens
 *
 */
char **tokenize(char *line)
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	for(i =0; i < strlen(line); i++){

		char readChar = line[i];

		if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
			token[tokenIndex] = '\0';
			if (tokenIndex != 0){
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
			}
		} else {
			token[tokenIndex++] = readChar;
		}
	}

	free(token);
	tokens[tokenNo] = NULL ;
	return tokens;
}


int main(int argc, char* argv[]) {
	char  line[MAX_INPUT_SIZE];            
	char  **tokens;              
	int i;
	char ttopword[]="ttop";

	FILE* fp;
	if(argc == 2) {
		fp = fopen(argv[1],"r");
		if(fp < 0) {
			printf("File doesn't exists.");
			return -1;
		}
	}

	while(1) {			
		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));
		if(argc == 2) { // batch mode
			if(fgets(line, sizeof(line), fp) == NULL) { // file reading finished
				break;	
			}
			line[strlen(line) - 1] = '\0';
		} else { // interactive mode
			printf("$ ");
			scanf("%[^\n]", line);
			getchar();
			sleep(1);
		}
		//printf("Command entered: %s (remove this debug output later)\n", line);
		/* END: TAKING INPUT */

		char *buf;
		char line1[64];
		char command[64];
		memset(command,0,sizeof(command));
		memset(line1,0,sizeof(line1));
		strcpy(line1, line);
		buf=strtok(line1," ");
		if(buf!=NULL)
			strcpy(command, buf);
		line[strlen(line)] = '\n'; //terminate with new line
		tokens = tokenize(line);

		//do whatever you want with the commands, here we just print them

		for(i=0;tokens[i]!=NULL;i++){
			//			printf("fifffound token %s (rer)\n", tokens[i]);
		}


		int pipeflag=0;
		//parsing piped commands
		if(strstr(line,"|")!=NULL){
			pipeflag=1;
			char* args[] = {0,0,0,0,0,0,0,0,0,0};
			char pipeline[64];
			memset(pipeline, 0, sizeof(pipeline));
			strcpy(pipeline, line);
			int a=0;
			char *pipetok;
			pipetok = strtok(pipeline, "|");
			while(pipetok!=NULL){
				args[a] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(args[a],pipetok);
				a++;
				pipetok = strtok(NULL,"|");
			}
			a--;

			int recur_fd[2];
			int output_size;
			char *buff[1000];//debug
			int pid;

			size_t pos=0;
			int in_fd=STDIN_FILENO;
			for(;a>=0;a--){
				int fd[2];
				//else{
				pipe(fd);
				int len;
				switch(fork()){
					case -1: 
						fprintf(stderr,"fork error\n");
						exit(1);
					case 0://child
						if(args[pos+1]==NULL){//last cmd 
							redirect(in_fd, STDIN_FILENO);
							tokens=tokenize(args[pos]);
							if(tokens[0]==NULL)
								printf("SSUShell : Incorrect command\n");
							if(execvp(tokens[0],tokens)<0){
								printf("SSUShell : Incorrect command\n");
								exit(-1);
							}
						}
						else{
							close(fd[0]);
							redirect(in_fd, STDIN_FILENO);
							redirect(fd[1], STDOUT_FILENO);
							tokens = tokenize(args[pos]);
							if(execvp(tokens[0],tokens)<0){
								printf("SSUShell : Incorrect command\n");
								exit(-1);
							}
							else
								printf("execvp succeed\n");
						}
					default: //parent
						close(fd[1]);
						//close(in_fd);
						tokens = tokenize(args[pos]);
						pos=pos+1;
						in_fd=fd[0];
						wait(NULL);
				}
			}//pipe fork closed
			}//strstr(line,"|") closed

			if(!pipeflag){
				int pid;
				if(buf!=NULL){
					if((pid=fork())<0){
						fprintf(stderr,"fork error\n");
						exit(1);
					}
					else if(pid==0){
						if(!strcmp(buf,ttopword)){
							 repeat_ttop();
							//run_ttop();
						}
						else if(strstr(buf,"pps")){
							run_pps(tokens);
						}
						else{
							printf("tok:%s\n",tokens[1]);
							if(execvp(buf, tokens)<0){
								printf("SSUShell : Incorrect command\n");
								exit(-1);
							}
						}
					}
					else{
						wait(NULL);
					}
				}
			}

		}//while(1) closed 
			// Freeing the allocated memory	
			for(i=0;tokens[i]!=NULL;i++){
				free(tokens[i]);
			}
			free(tokens);

		return 0;
	}
static void redirect(int oldfd, int newfd){
	if(oldfd!=newfd){
		if(dup2(oldfd, newfd)!=-1)
			close(oldfd);
		else
			perror("dup2");
	}
}
enum psopt{NONE,_A,_U,_X, _AU, _AX, _UX, _AUX};
void run_pps(char **buf){
get_mem(0);
get_psproc(NONE);
ppsopt_parse(buf);

}
int ppsopt_parse (char **buf){
	
	int aopt=0;
	int uopt=0;
	int xopt=0;
	int noneopt=0;
	int len;
	static char *optptr;

	if(buf[1]==NULL){
		noneopt=1;
	}
	else{
	optptr = buf[1];
	len=strlen(buf[1]);

	while(1){
		if(*optptr=='-'){
			printf("no such pps option.\n");
			return 0;
		}
		else if(*optptr=='a')
				aopt=1;
		else if(*optptr=='u')
				uopt=1;
		else if(*optptr=='x')
				xopt=1;
		if(!*++optptr)
			break;

	}
	}

	if(noneopt)
		get_ttyname();
	else if(aopt&&!uopt&&!xopt&&len==1)
		ppsprint_a();
	else if(uopt&&!aopt&&!xopt&&len==1)
		get_username(UOPT);
	else if(xopt&&!aopt&&!uopt&&len==1)
		get_username(XOPT);
	else if(aopt&&uopt&&xopt&&len==3)
		ppsprint_aux();
	else if(aopt&&xopt&&!uopt&&len==2)
		ppsprint_ax();
	else if(aopt&&uopt&&!xopt&&len==2)
		ppsprint_au();
	else if(!aopt&&uopt&&xopt&&len==2)
		get_username(UXOPT);
	else{
		printf("no such pps option.\n");
		return 0;
	}

}
void get_ttyname(){

	char *ret, tty[40];
	if((ret=ttyname(STDIN_FILENO))==NULL)
		perror("ttyname error");
	else{
		strcpy(tty,ret);
	}
	char *ptr=strrchr(tty,'p');
	ppsprint_none(ptr);
}

void get_username(int opt){
	FILE *fd;
	char filename[]="/etc/passwd";
	static const long max_len = 55+1;
	char buf[BUFSIZE];
	
	if((fd=fopen(filename, "rb"))!=NULL){
		fseek(fd,-max_len,SEEK_END);
		fread(buf, max_len-1, 1, fd);
		fclose(fd);

		buf[max_len-1]='\0';
		char *last_new_line = strrchr(buf,'\n');
		char *last_line=last_new_line+1;


	int len=strlen(last_line);
	char *ptr=strtok(last_line,":");

	if(opt==UOPT)
		ppsprint_u(ptr);
	if(opt==XOPT)
		ppsprint_x(ptr);
	if(opt==UXOPT)
		ppsprint_ux(ptr);

	}




}
void ppsprint_aux(){
	struct winsize w;//terminal size
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	printf("%6s %5s %3s %4s %5s %5s %5s %6s %6s %6s %s \n","USER","PID","%CPU","%MEM","VSZ","RSS","TTY","STAT","START","TIME","COMMAND");
	for(int l=0;l<totaltasks;l++){
			int cmdlen=w.ws_col;//custom with terminal column width
			char tmpcmd[BUFSIZE*10];
			memset(tmpcmd,0,BUFSIZE);
			char tmp[BUFSIZE];
			memset(tmp,0,BUFSIZE);
			sprintf(tmpcmd,"%6s %5s %3s %4s %5s %5s %5s %6s %6s %6s %s",ppsinfo[l][_USER],ppsinfo[l][_PID],ppsinfo[l][_CPU],ppsinfo[l][_MEM],ppsinfo[l][_VSZ],ppsinfo[l][_RSS], ppsinfo[l][_TTY],ppsinfo[l][_STAT],ppsinfo[l][_START], ppsinfo[l][_TIME], ppsinfo[l][_COMMAND]);
			strncpy(tmp,tmpcmd,cmdlen);
			printf("%s\n",tmp);
	}


}
void ppsprint_none(char *ttyname){
	printf("%5s %9s %6s %s \n","PID","TTY","TIME","COMMAND");
	char time[BUFSIZE]="00:00:00";
	for(int l=0;l<totaltasks;l++){
		if(!strcmp(ppsinfo[l][_TTY],ttyname))
			printf("%5s %9s %6s  %s \n",ppsinfo[l][_PID],ppsinfo[l][_TTY],time,ppsinfo[l][_COMMAND]);
	}



}
void ppsprint_a(){

	struct winsize w;//terminal size
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	printf("%5s %9s %6s %6s %s \n","PID","TTY","STAT","TIME","COMMAND");
	for(int l=0;l<totaltasks;l++){
		if(strcmp(ppsinfo[l][_TTY],"?")){
			int cmdlen=w.ws_col;//custom with terminal column width
			char tmpcmd[BUFSIZE*10];
			memset(tmpcmd,0,BUFSIZE);
			char tmp[BUFSIZE];
			memset(tmp,0,BUFSIZE);
			sprintf(tmpcmd,"%5s %9s %6s %6s %s",ppsinfo[l][_PID],ppsinfo[l][_TTY],ppsinfo[l][_STAT],ppsinfo[l][_TIME],ppsinfo[l][_COMMAND]);
			strncpy(tmp,tmpcmd,cmdlen);
			printf("%s\n",tmp);
		}
	}
}
void ppsprint_au(){
	struct winsize w;//terminal size
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	printf("%6s %6s %3s %4s %4s %5s %5s %6s %6s %6s %9s \n","USER","PID","%CPU","%MEM","VSZ","RSS","TTY","STAT","START","TIME","COMMAND");
	for(int l=0;l<totaltasks;l++){
		if(strcmp(ppsinfo[l][_TTY],"?")){
			int cmdlen=w.ws_col;//custom with terminal column width
			char tmpcmd[BUFSIZE*10];
			memset(tmpcmd,0,BUFSIZE);
			char tmp[BUFSIZE];
			memset(tmp,0,BUFSIZE);
			sprintf(tmpcmd,"%6s %6s %3s %4s %4s %5s %5s %6s %6s %6s %9s",ppsinfo[l][_USER],ppsinfo[l][_PID],ppsinfo[l][_CPU],ppsinfo[l][_MEM],ppsinfo[l][_VSZ],ppsinfo[l][_RSS], ppsinfo[l][_TTY],ppsinfo[l][_STAT],ppsinfo[l][_START], ppsinfo[l][_TIME], ppsinfo[l][_COMMAND]);
			strncpy(tmp,tmpcmd,cmdlen);
			printf("%s\n",tmp);
		}
	}

}
void ppsprint_ax(){
	struct winsize w;//terminal size
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	printf("%5s %9s %6s %6s %s \n","PID","TTY","STAT","TIME","COMMAND");
	for(int l=0;l<totaltasks;l++){
			int cmdlen=w.ws_col;//custom with terminal column width
			char tmpcmd[BUFSIZE*10];
			memset(tmpcmd,0,BUFSIZE);
			char tmp[BUFSIZE];
			memset(tmp,0,BUFSIZE);
			sprintf(tmpcmd,"%5s %9s %6s %6s %s",ppsinfo[l][_PID],ppsinfo[l][_TTY],ppsinfo[l][_STAT],ppsinfo[l][_TIME],ppsinfo[l][_COMMAND]);
			strncpy(tmp,tmpcmd,cmdlen);
			printf("%s\n",tmp);
		}
}
void ppsprint_u(char *username){
	struct winsize w;//terminal size
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	printf("%3s %5s %3s %4s %6s %6s %5s %5s %6s %6s %9s \n","USER","PID","%CPU","%MEM","VSZ","RSS","TTY","STAT","START","TIME","COMMAND");
	for(int l=0;l<totaltasks;l++){
		if(!strcmp(ppsinfo[l][_USER],username)){
		if(strcmp(ppsinfo[l][_TTY],"?")){
			int cmdlen=w.ws_col;//custom with terminal column width
			char tmpcmd[BUFSIZE*10];
			memset(tmpcmd,0,BUFSIZE);
			char tmp[BUFSIZE];
			memset(tmp,0,BUFSIZE);
			sprintf(tmpcmd,"%3s %5s %3s %4s %6s %6s %5s %5s %6s %6s %9s",ppsinfo[l][_USER],ppsinfo[l][_PID],ppsinfo[l][_CPU],ppsinfo[l][_MEM],ppsinfo[l][_VSZ],ppsinfo[l][_RSS], ppsinfo[l][_TTY],ppsinfo[l][_STAT],ppsinfo[l][_START], ppsinfo[l][_TIME], ppsinfo[l][_COMMAND]);
			strncpy(tmp,tmpcmd,cmdlen);
			printf("%s\n",tmp);
		}
		}
	}


}
void ppsprint_ux(char *username){
	struct winsize w;//terminal size
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	printf("%3s %5s %3s %4s %6s %6s %5s %6s %6s %6s %9s \n","USER","PID","%CPU","%MEM","VSZ","RSS","TTY","STAT","START","TIME","COMMAND");
	for(int l=0;l<totaltasks;l++){
		if(!strcmp(ppsinfo[l][_USER],username)){
			int cmdlen=w.ws_col;//custom with terminal column width
			char tmpcmd[BUFSIZE*10];
			memset(tmpcmd,0,BUFSIZE);
			char tmp[BUFSIZE];
			memset(tmp,0,BUFSIZE);
			sprintf(tmpcmd,"%3s %5s %3s %4s %5s %6s %5s %6s %6s %6s %9s",ppsinfo[l][_USER],ppsinfo[l][_PID],ppsinfo[l][_CPU],ppsinfo[l][_MEM],ppsinfo[l][_VSZ],ppsinfo[l][_RSS], ppsinfo[l][_TTY],ppsinfo[l][_STAT],ppsinfo[l][_START], ppsinfo[l][_TIME], ppsinfo[l][_COMMAND]);
			strncpy(tmp,tmpcmd,cmdlen);
			printf("%s\n",tmp);
		}
	}
}
void ppsprint_x(char *username){
	struct winsize w;//terminal size
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	printf("%5s %9s %6s %6s %s \n","PID","TTY","STAT","TIME","COMMAND");
	for(int l=0;l<totaltasks;l++){
		if(!strcmp(ppsinfo[l][_USER],username)){
			int cmdlen=w.ws_col;//custom with terminal column width
			char tmpcmd[BUFSIZE*10];
			memset(tmpcmd,0,BUFSIZE);
			char tmp[BUFSIZE];
			memset(tmp,0,BUFSIZE);
			sprintf(tmpcmd,"%5s %9s %6s %6s %s",ppsinfo[l][_PID],ppsinfo[l][_TTY],ppsinfo[l][_STAT],ppsinfo[l][_TIME],ppsinfo[l][_COMMAND]);
			strncpy(tmp,tmpcmd,cmdlen);
			printf("%s\n",tmp);
		}
	}
}

void handle_alarm(int sig){
	print_flag=true;
}
static int mcol, mrow;
void repeat_ttop() {
	int ch;
	WINDOW *new;
	new=initscr();
	keypad(stdscr,TRUE);

	getmaxyx(stdscr, mrow, mcol);

	curs_set(0);

	

	int rowcount=350;
	new=newpad(rowcount+1,mcol);

		if(signal(SIGALRM, handle_alarm)==SIG_ERR){
			fprintf(stderr,"SIGALRM error\n");
			exit(1);
		}
		alarm(1);
		//curs_set(FALSE);
		int i=0;
		while(1){
			if(print_flag){
				run_ttop();
				print_flag=false;
				alarm(3);
			}
	
		}
	endwin();
	refresh();


}
void run_ttop(){
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	int ch;
	/*while(1){
		if(getch()!='q')
			break;*/
	//(first line)
	//local time
	mvprintw(0,0,"top - ");
	mvprintw(0,6,"%02d:%02d:%02d ",tm.tm_hour,tm.tm_min,tm.tm_sec);
	//uptime
	mvprintw(0,16,"up ");
	get_uptime(1);
	mvprintw(0,25,",  ");
	//active user num
	get_user();
	refresh();
	//load average
	get_loadav();
	//tasks(second line)
	mvprintw(1,0,"Tasks: ");
	refresh();
	get_procnum(0);
	//cpu(third line)
	get_cpu();
	//mem(ram memory)(fourth line)
	get_mem(1);
	//pid user...
	get_graph();
	//sleep(3);
	//}*/

}
void get_graph(){
	mvprintw(5,0,"%7s %-9s %-4s %-6s %6s %4s %9s %6s %6s %6s %9s %6s","PID","USER","PR","NI","VIRT","RES","SHR","S","%%CPU","%MEM","TIME+","COMMAND\n");
	get_procnum(1);
	int cnt=6;
	for(int l=0;l<totaltasks;l++){
	mvprintw(cnt,0,"%7s %-9s %-4s %-6s %6s %4s %9s %6s %6s %6s %9s %6s\n",ttopgraph[l][PID],ttopgraph[l][GUSER],ttopgraph[l][PR],ttopgraph[l][NI],ttopgraph[l][VIRT],ttopgraph[l][RES],ttopgraph[l][SHR],ttopgraph[l][S],ttopgraph[l][CPU],ttopgraph[l][MEM],ttopgraph[l][TIME],ttopgraph[l][COMMAND]);
	cnt++;
	}
	refresh();
				//ttopgraph[a][PID] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));

	
		/*for(int j=0;j<=totprocess;j++){
			for(int i=0;i<12;i++){
				free(ttopgraph[j][i]);
			}
		}*/
		//free(ttograph);


}
	
int get_uptime(int ttopprint){
	FILE *fp;
	char buf[36];
	char uptimechr[28];
	//uptime
	if((fp=fopen("/proc/uptime","r"))==NULL){
		perror("supt");
		exit(-1);
	}
	fgets(uptimechr,12,fp);
	fclose(fp);
	//stime idletime
	double stime, idletime;
	fp=fopen("/proc/uptime","r");
	fgets(buf,36,fp);
	sscanf(buf,"%lf %lf",&stime,&idletime);
	fclose(fp);

	uptime=strtol(uptimechr,NULL,10);

	long days=uptime/(60*60*24);
	uptime -= days*(60*60*24);
	long hours=uptime/(60*60);
	uptime -= hours*(60*60);
	long mins=uptime/60;

	if(ttopprint){
	if(days>0)//day
		mvprintw(0,20,"%ld days",days);
	else if(hours>0)//hrs
		mvprintw(0,20,"%ld:%02ld",hours,mins);
	else if(mins>0)//min
		mvprintw(0,20,"%ld min",mins);
	else//sec
		mvprintw(0,20,"%ld sec",uptime);
	}

		return (int)stime;

}
void get_pstty(int tty_maj,int tty_min,int k){
	ppsinfo[k][_TTY] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
	memset(ppsinfo[k][_TTY],0,MAX_TOKEN_SIZE);
char ps_tty[BUFSIZE]={0};
if(tty_maj==136)
	sprintf(ps_tty, "pts/%d", tty_min);
else if(tty_maj==4)
	sprintf(ps_tty, "tty%d", tty_min);
else
	strcpy(ps_tty,"?");	

strcpy(ppsinfo[k][_TTY], ps_tty);

}

void get_user(){
	char *s, *c;
	struct utmp *u;
	int i;
	c=getlogin();
	setutent();
	u=getutent();
	int cnt=0;
	while(u!=NULL){
		if(u->ut_type==7&&strcmp(u->ut_user,c)==0){
			cnt++;
			mvprintw(0,27,"%d user,",cnt);
	refresh();
		}
		u=getutent();
	}
}
void get_loadav(){
	double load[3];
	if(getloadavg(load,3) != -1)
		mvprintw(0,35,"  load average : %0.2f, %0.2f, %0.2f\n",load[0],load[1],load[2]);
	refresh();
}
void get_psuser(char* statfile, int k){
	struct passwd *upasswd;
	char username[BUFSIZE];
	struct stat lstat;
	stat(statfile,&lstat);
	upasswd=getpwuid(lstat.st_uid);
	strncpy(username, upasswd->pw_name, 32);
		ppsinfo[k][_USER] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		memset(ppsinfo[k][_USER],0,MAX_TOKEN_SIZE);
		strcpy(ppsinfo[k][_USER],username);
}
//enum ppsinfo{_PID, _USER, _TTY, _STAT, _TIME, _COMMAND, _CPU, _MEM, _VSZ, _RSS, _START};
void get_psproc(int psopt){
	size_t procnum;
	DIR* dir;
	struct dirent *dirp;
	if(!(dir=opendir("/proc")))
		perror("opendir error\n");
	int digflag;
	int a=0;
	int cnt=0;
	int major, minor;
	while((dirp=readdir(dir))!=NULL){
		if(!strcmp(dirp->d_name,".")&&!strcmp(dirp->d_name,".."))
			continue;
		if(dirp->d_type!=DT_DIR)
			continue;
		char c[BUFSIZE]={0};
		strcpy(c, dirp->d_name);

		for(int i=0;c[i]!='\0';i++){
			if(c[i]=='0'||c[i]=='1'||c[i]=='2'||c[i]=='3'||c[i]=='4'||c[i]=='5'||c[i]=='6'||c[i]=='7'||c[i]=='8'||c[i]=='9')
				digflag=1;
			else{
				digflag=0;
				break;
			}
		}
		if(!digflag)
			continue;
		else{
				ppsinfo[a][_PID] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ppsinfo[a][_PID],0,MAX_TOKEN_SIZE);
				strcpy(ppsinfo[a][_PID],c);
				a++;
		}
		++cnt;
	}
	totaltasks=cnt;
	if(closedir(dir))
		perror("closedir error\n");
	//PID all saved 

	int k=0;
	int b=a;
	a=0;
	while(1){
	a++;
	if(a==b)
		break;
	//USER 
	char statfile[BUFSIZE];
	memset(statfile,0,BUFSIZE);
	sprintf(statfile,"/proc/%s/stat",ppsinfo[k][_PID]);
	if(access(statfile,F_OK)!=0)
		continue;
	get_psuser(statfile,k);
				
	FILE *fp;
	char tmp[BUFSIZE];
	fp = fopen(statfile,"r");
	for(int i=1;i<=2;i++)
		fscanf(fp,"%s",tmp);

	char state[BUFSIZE];
	memset(state,0,BUFSIZE);
	fscanf(fp,"%s",state);//3

	int end=strlen(state);

		fscanf(fp,"%s",tmp);
	int pgrp[BUFSIZE];
		fscanf(fp,"%d",&pgrp[k]);//5
	int session[BUFSIZE];
		fscanf(fp,"%d",&session[k]);//6
	int tty_nr[BUFSIZE];
	fscanf(fp,"%d",&tty_nr[k]);//7
	tty_maj[k]=MAJOR(tty_nr[k]);
	tty_min[k]=MINOR(tty_nr[k]);
	get_pstty(tty_maj[k],tty_min[k],k);
	
	int tpgid[BUFSIZE];
		fscanf(fp,"%d",&tpgid[k]);

		int plusflag=0;

	for(int i=1;i<=5;i++)
		fscanf(fp,"%s",tmp);
	int utime[BUFSIZE];//14
		fscanf(fp,"%d",&utime[k]);
	int stime[BUFSIZE];//15
		fscanf(fp,"%d",&stime[k]);
	int cutime[BUFSIZE];//16
		fscanf(fp,"%d",&cutime[k]);
	int cstime[BUFSIZE];//17
		fscanf(fp,"%d",&cstime[k]);//17
	fscanf(fp,"%s",tmp);
	int nice[BUFSIZE];
		fscanf(fp,"%d",&nice[k]);//19

	if(nice[k]>0){
		state[end++]='N';
		state[end++]='\0';
	}
	else if(nice[k]<0){
		state[end++]='<';
		state[end++]='\0';
	}

	for(int i=1;i<=2;i++)
		fscanf(fp,"%s",tmp);
	int starttime[BUFSIZE];
		fscanf(fp,"%d",&starttime[k]);//22

	ppsinfo[k][_VSZ] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		memset(ppsinfo[k][_VSZ],0,MAX_TOKEN_SIZE);
	ppsinfo[k][_RSS] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		memset(ppsinfo[k][_RSS],0,MAX_TOKEN_SIZE);

		//cpu time
	int seconds=get_uptime(0);

	time_t startsec;
	time_t seconds_ago;
	seconds_since_1970=time(NULL);

		ppsinfo[k][_START] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		memset(ppsinfo[k][_START],0,MAX_TOKEN_SIZE);

	struct tm *proc_time;
	struct tm *our_time;
	time_t t;
	const char *fmt;
	int tm_year;
	int tm_yday;
	size_t len;
	our_time=localtime(&seconds_since_1970);
	tm_year=our_time->tm_year;
	tm_yday = our_time->tm_yday; //date of today 
	t= seconds_since_1970 - seconds + starttime[k]/sysconf(_SC_CLK_TCK);//calculate START TIME
	proc_time=localtime(&t);
	fmt="%H:%M";
	if(tm_yday!=proc_time->tm_yday) {
		fmt="%b%d";
	}
	if(tm_year!=proc_time->tm_year) fmt="%Y";
	len=strftime(ppsinfo[k][_START],240,fmt,proc_time);
	if(len<=0 || len>=240) ppsinfo[k][_START][len=0] = '\0';
		int pcpu=get_cputime(utime[k],stime[k],starttime[k],seconds);
		int total_cpujiff=utime[k]+stime[k];
		//printf("total:%d  ",total_time);
		double totalsec=(double)total_cpujiff/HZ;
		struct timespec val;
		jiffies_to_timespec(pcpu, &val);
		int totalmin;
		if(totalsec>60){
			totalmin=totalsec/60;
			totalsec-=totalmin*60;
		}
		ppsinfo[k][_TIME] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		memset(ppsinfo[k][_TIME],0,MAX_TOKEN_SIZE);
		sprintf(ppsinfo[k][_TIME],"%02d:%02d",totalmin,(int)totalsec);

		//double cpu_usage=100*(((double)total_time/(double)HZ)/seconds);
		//printf("uptime:%ld  ",uptime);
		//printf("sec:%f  ",seconds);
				ppsinfo[k][_CPU] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ppsinfo[k][_CPU],0,MAX_TOKEN_SIZE);
				//sprintf(ppsinfo[k][_CPU],"%0.1f",pcpu%10);

		int total_time=utime[k]+stime[k]+cutime[k]+cstime[k];
		jiffies_to_timespec(total_time, &val);
		double cpu_usage=100*(((double)total_time/(double)HZ)/seconds);
				if(cpu_usage<0)
					strcpy(ppsinfo[k][_CPU],"0.0");
				else
					sprintf(ppsinfo[k][_CPU],"%0.1f",cpu_usage);
		fclose(fp);//proc/pid/stat end
	//COMMAND
	char cmdfile[BUFSIZE];
	memset(cmdfile,0,BUFSIZE);
	sprintf(cmdfile,"/proc/%s/cmdline",ppsinfo[k][_PID]);
	FILE *cmdfp;
	cmdfp=fopen(cmdfile,"r");

				ppsinfo[k][_COMMAND] = (char*)malloc(4*MAX_TOKEN_SIZE*sizeof(char));
				memset(ppsinfo[k][_COMMAND],0,4*MAX_TOKEN_SIZE);
	char cmdline[BUFSIZE];
	memset(cmdline,0,BUFSIZE);
	
	//fscanf(cmdfp,"%s", ppsinfo[k][_COMMAND]);
	fscanf(cmdfp,"%s", cmdline);
	strcpy(ppsinfo[k][_COMMAND],cmdline);
	fclose(cmdfp);

	int kpid=atoi(ppsinfo[k][_PID]);


	//proc/pid/status
	open_status(k,session[k],state,cmdline);

	//proc/pid/task dir (checkout multi thread)
	open_task(k,kpid,state,plusflag);

	end=strlen(state);
	if(tpgid[k]==pgrp[k]){
		state[end++]='+';
		state[end++]='\0';
	}

	ppsinfo[k][_STAT] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
	memset(ppsinfo[k][_STAT],0,MAX_TOKEN_SIZE);
	strcpy(ppsinfo[k][_STAT],state);

	k++;
	}//while(1) ends 

	/*	for(int j=0;j<=k;j++){
			for(int i=0;i<3;i++){
				free(ppsinfo[j][i]);
			}
		}*/
}
void open_task(int k,int kpid, char *procstate,int plusflag){
	DIR* dir;
	struct dirent *dirp;
	char taskdir[BUFSIZE];
	memset(taskdir,0,BUFSIZE);
	sprintf(taskdir,"/proc/%d/task",kpid);//ppsinfo[i][_PID]);
	int cnt=0;
	
	if(!(dir=opendir(taskdir)))
		perror("opendir error\n");
	while((dirp=readdir(dir))!=NULL){
		if(!strcmp(dirp->d_name,".")||!strcmp(dirp->d_name,".."))
			continue;

		++cnt;
	}

	int end=strlen(procstate);
	

	
	if(cnt>1){
		procstate[end++]='l';
		procstate[end++]='\0';
	}

	if(closedir(dir))
		perror("closedir error\n");



}
void open_status(int k, int session, char* procstate, char* cmdline){
	FILE *fp;
	char statusfile[BUFSIZE];
	char tmp[BUFSIZE];
	int tmpi[BUFSIZE];
	memset(statusfile,0,BUFSIZE);
	sprintf(statusfile,"/proc/%s/status",ppsinfo[k][_PID]);
	fp = fopen(statusfile,"r");
		fscanf(fp,"%s",tmp);

	if(strlen(cmdline)==0){
		fscanf(fp,"%s",cmdline);
		sprintf(ppsinfo[k][_COMMAND],"[%s]",cmdline);
	}
	else
		fscanf(fp,"%s",tmp);

	for(int i=1;i<=6;i++)
		fscanf(fp,"%s",tmp);
	int tgid[BUFSIZE];
	fscanf(fp,"%d",&tgid[k]);//9
	int end=strlen(procstate);
	
	if(session==tgid[k]){
		procstate[end++]='s';
		procstate[end++]='\0';
	}
				ppsinfo[k][_MEM] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ppsinfo[k][_VSZ],0,MAX_TOKEN_SIZE);
				memset(ppsinfo[k][_RSS],0,MAX_TOKEN_SIZE);
				memset(ppsinfo[k][_MEM],0,MAX_TOKEN_SIZE);
	int vmsizeyes=0;
	for(int i=1;i<=41;i++){
		fscanf(fp,"%s",tmp);
	if(strstr(tmp,"VmSize:")){
		vmsizeyes=1;
		fscanf(fp,"%s",ppsinfo[k][_VSZ]);
	fscanf(fp,"%s",tmp);
	if(strstr(tmp,"VmLck:")){
		fscanf(fp,"%d",tmpi);
		if(strcmp(tmp,"0")){
		procstate[end++]='L';
		procstate[end++]='\0';
		}
	}
	else
		fscanf(fp,"%s",tmp);

	for(int i=1;i<=9;i++)
		fscanf(fp,"%s",tmp);
	if(strstr(tmp,"VmRSS:"))
		fscanf(fp,"%s",ppsinfo[k][_RSS]);
	int res=atoi(ppsinfo[k][_RSS]);
	double memper=(double)res/(double)totalram;
	sprintf(ppsinfo[k][_MEM],"%.1f",memper);
	for(int i=1;i<=5;i++)
		fscanf(fp,"%s",tmp);
	break;
	}
	}//for 41 end
	if(!vmsizeyes){
		strcpy(ppsinfo[k][_VSZ],"0");
		strcpy(ppsinfo[k][_RSS],"0");
		strcpy(ppsinfo[k][_MEM],"0.0");
	} 

	fclose(fp);
}
		
//enum ppsinfo{_PID, _USER, _TTY, _STAT, _TIME, _COMMAND, _CPU, _MEM, _VSZ, _RSS, _START};
int get_cputime(ulong utime, ulong stime, ulong starttime, int seconds){
	unsigned long long total_time;
	int pcpu=0;

	total_time=utime+stime;
		//--double seconds=(double)uptime-((double)starttime[k]/HZ);
	seconds=seconds-(int)(starttime/100);
	if(seconds)
		pcpu=(int)(total_time * 1000ULL/100.)/seconds;
	return pcpu;
}

void get_procnum(int graphflag){
	size_t procnum;
	DIR* dir;
	struct dirent *dirp;
	if(!(dir=opendir("/proc")))
		perror("opendir error\n");
	size_t cnt=0;
	int digflag;
	int a=0;
	while((dirp=readdir(dir))!=NULL){
		if(!strcmp(dirp->d_name,".")&&!strcmp(dirp->d_name,".."))
			continue;
		if(dirp->d_type!=DT_DIR)
			continue;
		char c[BUFSIZE]={0};
		strcpy(c, dirp->d_name);

		for(int i=0;c[i]!='\0';i++){
			if(c[i]=='0'||c[i]=='1'||c[i]=='2'||c[i]=='3'||c[i]=='4'||c[i]=='5'||c[i]=='6'||c[i]=='7'||c[i]=='8'||c[i]=='9')
				digflag=1;
			else{
				digflag=0;
				break;
			}
		}
		if(!digflag)
			continue;
		else{
				ttopgraph[a][PID] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[a][PID],0,MAX_TOKEN_SIZE);
				strcpy(ttopgraph[a][PID],c);
				a++;
		}


		++cnt;

	}
	totaltasks=cnt;
	if(!graphflag){
		mvprintw(1,6,"%ld total,  ",cnt);
	refresh();
	}
	if(closedir(dir))
		perror("closedir error\n");

	FILE *fp;
	char buf[BUFSIZE];
	int runcnt=0;
	int sleepcnt=0;
	int stopcnt=0;
	int zombiecnt=0;
	int b=a;
	char statfile[BUFSIZE];
	char username[BUFSIZE];
	struct stat lstat;
	a=0;
	int k=0;
	char tmp[BUFSIZE];
	int flagtmp=1;
	while(1){
	memset(buf,0,BUFSIZE);
	sprintf(buf,"/proc/%s/status",ttopgraph[k][PID]);
	a++;
	if(a==b)
		break;
		fp = fopen(buf,"r");
	char  line[MAX_INPUT_SIZE];
	while(fgets(line,sizeof(line),fp)){
		if(strstr(line,"run"))
			runcnt++;
		else if(strstr(line,"sleeping"))
			sleepcnt++;
		else if(strstr(line,"stopped"))
			stopcnt++;
		else if(strstr(line,"zombie"))
			zombiecnt++;
	}
	fclose(fp);

	if(graphflag){
	char cmdfile[BUFSIZE];
	memset(cmdfile,0,BUFSIZE);
	sprintf(cmdfile,"/proc/%s/cmdline",ttopgraph[k][PID]);
	FILE *cmdfp;
	cmdfp=fopen(cmdfile,"r");
				ttopgraph[k][COMMAND] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[k][COMMAND],0,MAX_TOKEN_SIZE);
	char cmdline[BUFSIZE];
	memset(cmdline,0,BUFSIZE);
	//fscanf(cmdfp,"%s", ttopgraph[k][COMMAND]);
	fscanf(cmdfp,"%s", cmdline);
	char cmdtmp[BUFSIZE];
	memset(cmdtmp,0,BUFSIZE);
	strncpy(cmdtmp,cmdline,17);
	sprintf(ttopgraph[k][COMMAND],"%s+",cmdtmp);
	//printf("cmd:%sj\n",cmdline);
	fclose(cmdfp);
	char statusfile[BUFSIZE];
	memset(statusfile,0,BUFSIZE);
	sprintf(statusfile,"/proc/%s/status",ttopgraph[k][PID]);

		fp = fopen(statusfile,"r");
	fscanf(fp,"%s",tmp);

	if(strlen(cmdline)==0){
	fscanf(fp,"%s",cmdline);
	strncpy(cmdtmp,cmdline,17);
	sprintf(ttopgraph[k][COMMAND],"[%s]",cmdtmp);
	}
	else
	fscanf(fp,"%s",tmp);

	for(int i=1;i<=3;i++)
		fscanf(fp,"%s",tmp);
				ttopgraph[k][S] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[k][S],0,MAX_TOKEN_SIZE);
	fscanf(fp,"%s",ttopgraph[k][S]);
				ttopgraph[k][VIRT] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				ttopgraph[k][RES] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				ttopgraph[k][SHR] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				ttopgraph[k][MEM] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[k][VIRT],0,MAX_TOKEN_SIZE);
				memset(ttopgraph[k][RES],0,MAX_TOKEN_SIZE);
				memset(ttopgraph[k][SHR],0,MAX_TOKEN_SIZE);
				memset(ttopgraph[k][MEM],0,MAX_TOKEN_SIZE);
	int vmsizeyes=0;
for(int i=1;i<=36;i++){
		fscanf(fp,"%s",tmp);
	if(strstr(tmp,"VmSize:")){
		vmsizeyes=1;
		fscanf(fp,"%s",ttopgraph[k][VIRT]);
	for(int i=1;i<=11;i++)
		fscanf(fp,"%s",tmp);
	if(strstr(tmp,"VmRSS:"))
		fscanf(fp,"%s",ttopgraph[k][RES]);
	int res=atoi(ttopgraph[k][RES]);
	double memper=(double)res/(double)totalram;
	sprintf(ttopgraph[k][MEM],"%0.1f",memper);
	for(int i=1;i<=5;i++)
		fscanf(fp,"%s",tmp);
		fscanf(fp,"%s",ttopgraph[k][SHR]);
		break;
	}
}
	if(!vmsizeyes){
		strcpy(ttopgraph[k][VIRT],"0");
		strcpy(ttopgraph[k][RES],"0");
		strcpy(ttopgraph[k][SHR],"0");
		strcpy(ttopgraph[k][MEM],"0.0");
	} 
	fclose(fp);

	memset(statfile,0,BUFSIZE);
	sprintf(statfile,"/proc/%s/stat",ttopgraph[k][PID]);
	if(access(statfile,F_OK)!=0)
		continue;
	struct passwd *upasswd;
	stat(statfile,&lstat);
	upasswd=getpwuid(lstat.st_uid);
	strncpy(username, upasswd->pw_name, 32);
				ttopgraph[k][GUSER] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[k][GUSER],0,MAX_TOKEN_SIZE);
				strcpy(ttopgraph[k][GUSER],username);
	fp = fopen(statfile,"r");
	for(int i=1;i<=13;i++)
		fscanf(fp,"%s",tmp);
	int utime[BUFSIZE];//[2]={{0}};//14
		fscanf(fp,"%d",&utime[k]);
	int stime[BUFSIZE];//[2]={{0}};
		fscanf(fp,"%d",&stime[k]);
	int cutime[BUFSIZE];//[2]={{0}};
		fscanf(fp,"%d",&cutime[k]);
	int cstime[BUFSIZE];//[2]={{0}};//17
		fscanf(fp,"%d",&cstime[k]);
		int total_time=utime[k]+stime[k]+cutime[k]+cstime[k];
		int total_cpujiff=utime[k]+stime[k];
		double totalsec=(double)total_cpujiff/HZ;
		struct timespec val;
		jiffies_to_timespec(total_time, &val);

				ttopgraph[k][TIME] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[k][TIME],0,MAX_TOKEN_SIZE);
		sprintf(ttopgraph[k][TIME],"0:%05.02f",totalsec);

				ttopgraph[k][PR] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[k][PR],0,MAX_TOKEN_SIZE);
				ttopgraph[k][NI] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[k][NI],0,MAX_TOKEN_SIZE);
	fscanf(fp,"%s",ttopgraph[k][PR]);//18
	fscanf(fp,"%s",ttopgraph[k][NI]);//19
	for(int i=1;i<=2;i++)
		fscanf(fp,"%s",tmp);
	int starttime[BUFSIZE];//[2]={{0}};
		fscanf(fp,"%d",&starttime[k]);//22
		double seconds=(double)uptime-((double)starttime[k]/HZ);
		double cpu_usage=100*(((double)total_time/(double)HZ)/seconds);
				ttopgraph[k][CPU] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				memset(ttopgraph[k][CPU],0,MAX_TOKEN_SIZE);
				if(cpu_usage<0)
					strcpy(ttopgraph[k][CPU],"0.0");
				else
					sprintf(ttopgraph[k][CPU],"%0.1f",cpu_usage);
		//printf("i*******cpu:%s\n",ttopgraph[k][CPU]);
	fclose(fp);
	k++;
	}//if(graphflag) done
	/*int l=k;//debugging ncurses....
	mvprintw(6,0,"%7s %-9s %-4s %-6s %6s %4s %9s %6s %6s %6s %9s %6s\n",ttopgraph[l][PID],ttopgraph[l][GUSER],ttopgraph[l][PR],ttopgraph[l][NI],ttopgraph[l][VIRT],ttopgraph[l][RES],ttopgraph[l][SHR],ttopgraph[l][S],ttopgraph[l][CPU],ttopgraph[l][MEM],ttopgraph[l][TIME],ttopgraph[l][COMMAND]);
	//	mvprintw(6,0,"hi%d\n",k);
		refresh();*/
	}//while(1) done

	totprocess=k;
	

	if(!graphflag){
		mvprintw(1,18,"%d running,  %d sleeping,   %d stopped,   %d zombie\n",runcnt,sleepcnt,stopcnt,zombiecnt);
		refresh();
	}

}

static void jiffies_to_timespec(unsigned long jiffies, struct timespec *value){
	value->tv_nsec=(jiffies%HZ)*(1000000000L / HZ);
	value->tv_sec=jiffies/HZ;
	//printf("value:%ld   ",value->tv_sec);
	//printf("nvalue:%ld\n",value->tv_nsec);
}
enum jiffy{USER, USER_NICE, SYSTEM, IDLE,IOWAIT,HI,SI,ST} jiffy_enum;
void get_cpu(){//ttop third line 
	FILE *fp;
	char tmp[BUFSIZE*10];
	char cpustat[BUFSIZE*10]={0};
	int i=0;
	char cpuId[8]={0};
	int jiff[2][8]={0};
	int totaljiff[2]={0};
	int difftotal;
	int diffjiff[8]={0};
//	while(1){
		fp=fopen("/proc/stat","r");

		fscanf(fp,"%s %d %d %d %d %d %d %d %d",cpuId,&jiff[1][USER],&jiff[1][USER_NICE],&jiff[1][SYSTEM],&jiff[1][IDLE],&jiff[1][IOWAIT],&jiff[1][HI],&jiff[1][SI],&jiff[1][ST]);
		totalcpuusage=jiff[1][USER]+jiff[1][USER_NICE]+jiff[1][SYSTEM]+jiff[1][IDLE];
		for(int idx=0;idx<4;++idx){//total cputime(user+system)
			diffjiff[idx]=jiff[1][idx]-jiff[0][idx];
			totaljiff[1]=totaljiff[1]+diffjiff[idx];
			difftotal=totaljiff[1]-totaljiff[0];
		}
			//mvprintw(1,30,"\n");
		mvprintw(2,0,"%%Cpu(s):  ");
		mvprintw(2,10,"%.1lf us,  ",100.0*(double)diffjiff[USER]/(double)difftotal);
		mvprintw(2,19,"%.1lf sy,  ",100.0*(double)diffjiff[SYSTEM]/(double)difftotal);
		mvprintw(2,28,"%.1lf ni,  ",100.0*(double)diffjiff[USER_NICE]/(double)difftotal);
		mvprintw(2,36,"%.1lf id,  ",100.0*(double)diffjiff[IDLE]/(double)difftotal);
		mvprintw(2,45,"%.1lf wa,  ",100.0*(double)diffjiff[IOWAIT]/(double)difftotal);
		mvprintw(2,54,"%.1lf hi,  ",100.0*(double)diffjiff[HI]/(double)difftotal);
		mvprintw(2,63,"%.1lf si,  ",100.0*(double)diffjiff[SI]/(double)difftotal);
		mvprintw(2,72,"%.1lf st\n",100.0*(double)diffjiff[ST]/(double)difftotal);
		refresh();
		memcpy(jiff[0],jiff[1],sizeof(int)*8);
		totaljiff[0]=totaljiff[1];

	fclose(fp);
//	sleep(3);//3sec update 
//	}
}
enum memory{TOTAL, FREE, AVAIL, USED, BUFF, CACHE, SWCACHE, SWtotal, SWfree, SWused, SLAB, SRec};
void get_mem(int ttopflag){
	FILE *fp;
	char tmp1[BUFSIZE];
	char tmp2[BUFSIZE];
	char tmp3[BUFSIZE];
	int mem[15]={0};
	fp=fopen("/proc/meminfo","r");
	fscanf(fp,"%s %d %s",tmp1,&mem[TOTAL],tmp2);
	fscanf(fp,"%s %d %s",tmp1,&mem[FREE],tmp2);
	
	fscanf(fp,"%s %d %s",tmp1,&mem[AVAIL],tmp3);
	
	mem[BUFF]=0;
	fscanf(fp,"%s %d %s",tmp1,&mem[BUFF],tmp2);
	if(ttopflag){
	mvprintw(3,0,"KiB Mem:  %d total,   %d free,   ",mem[TOTAL],mem[FREE]);
	refresh();
	}
	int bu=mem[BUFF];
	fscanf(fp,"%s %d %s",tmp1,&mem[CACHE],tmp2);
	int cac=mem[CACHE];

	fscanf(fp,"%s %d %s",tmp1,&mem[SWCACHE],tmp2);
	for(int i=1;i<9;i++)
		fscanf(fp,"%s %s %s",tmp1,tmp2,tmp3);
	fscanf(fp,"%s %d %s",tmp1,&mem[SWtotal],tmp2);
	int swt=mem[SWtotal];
	fscanf(fp,"%s %d %s",tmp1,&mem[SWfree],tmp2);

	for(int i=1;i<7;i++)
		fscanf(fp,"%s %s %s",tmp1,tmp2,tmp3);
	fscanf(fp,"%s %d %s",tmp1,&mem[SLAB],tmp2);
	fscanf(fp,"%s %d %s",tmp1,&mem[SRec],tmp2);
	mem[USED]=mem[TOTAL]-mem[FREE]-bu-cac-mem[SRec];
	if(ttopflag){
	mvprintw(3,45,"%d used,   ",mem[USED]);
	refresh();
	}
	int bc=bu+cac+mem[SRec];
	if(ttopflag){
	mvprintw(3,60,"%d buff/cache\n",bc);
	refresh();

	mvprintw(4,0,"KiB swap:  %d total,   ",swt);
	mvprintw(4,27,"%d free,   ",mem[SWfree]);
	refresh();
	}
	mem[SWused]=mem[SWtotal]-mem[SWfree];
	if(ttopflag){
	mvprintw(4,45,"%d used.   ",mem[SWused]);
	mvprintw(4,60,"%d avail Mem\n\n",mem[AVAIL]);
	refresh();
	}
	
	totalram=mem[TOTAL]+mem[SWtotal];
	//printf("-----ram:%d\n",totalram);

}
		

