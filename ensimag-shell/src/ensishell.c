/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variante.h"
#include "readcmd.h"

#include <sys/wait.h>

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

struct listProcess * lProcess;

struct process * find(pid_t pid){
	struct process * cour = lProcess->premier;
	while(cour!= NULL && cour->pid!=pid){
		cour = cour->suivant;
	}
	return cour;
}
/*A signal was caught, indicating that child has finished */
void sigHandler(int sig) {
    pid_t p;
    int status;
    if (sig==SIGCHLD && (p=waitpid(-1, &status, WNOHANG)) > 0 ) {
		struct process * proc = find(p);
		if(proc!=NULL){
			proc->completed=1;
			
		}
    }   
}


/* Question 4 :Dans cette fonction on liste les processus en tâche 
 * de fond dans struct listProcess * lProcess.
 * */
void jobsList(pid_t pid,char *** seq){

	struct process * p = malloc(sizeof(struct process));
	if(p == NULL)
		exit(EXIT_FAILURE);
	p->pid = pid;
	p->completed = 0;
	int leng;
	for (leng=0; seq[leng]!=0; leng++){}
	p->seq = malloc(leng*sizeof(char **));
	for (int i=0; i<leng; i++) {
		int l = 0;
		for (int j=0; seq[i][j]!=0; j++) {l++;}
		p->seq[i]=malloc(l*sizeof(char*));
		for (int j=0; j<l; j++) {
			p->seq[i][j] = malloc(strlen(seq[i][j]));
			strcpy(p->seq[i][j],seq[i][j]);	
		}
		p->seq[i][l]=NULL;
	}
	p->seq[leng]=NULL;
	p->suivant = lProcess->premier;
	lProcess->premier =p;
}
/* On a affiche les processus de la liste des processus
 * lProcessus et leurs états.
 */

void jobs(){
	struct process* courant = lProcess->premier;
	while(courant!=NULL){
		printf("[%d]       ",courant->pid);
		for (int i=0; courant->seq[i]!=0; i++) {
			for (int j=0; courant->seq[i][j]!=0; j++) {
				printf("%s ", courant->seq[i][j]);
			}
	   }
	   if(courant->completed)
			printf("    completed\n");
		else
			printf("    running\n");
		courant = courant->suivant;
	}
}
/*Question 5:  Pipe */ 
void pipeFunction(struct cmdline * l){
	int res;
	int tuyau[2];
	pipe(tuyau);
	switch( res=fork() ){
	case -1:
		perror("fork:");
		break;
	case 0:
		dup2(tuyau[0], 0);
		close(tuyau[1]);
		close(tuyau[0]);
		execvp(l->seq[1][0],l->seq[1]);
		break;
	default:
		dup2(tuyau[1], 1);
		close(tuyau[0]); 
		close(tuyau[1]);
		execvp(l->seq[0][0],l->seq[0]);
		break;
	}
}
/* Question 6 : Redirection */

void redirection(struct cmdline * l){
	int in, out;
	in = open(l->in, O_RDONLY);
	out = open(l->out, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
	dup2(in,0);
	dup2(out,1);
	close(in);
	close(out);
}



/* Question 1,2 et 3 */
void lireEtExecuter(struct cmdline * l){

	pid_t pid;
	switch( pid = fork() ) {
	case -1:
	  perror("fork:");
	  break;
	case 0:
	  if(strcmp(l->seq[0][0],"jobs")==0){
		jobs();
	  }else{
			if(l->in) 
				redirection(l); //appelle de la fonction de redirection dans les fichiers
			
			execvp(l->seq[0][0],l->seq[0]);
		}
	  break;	
	default:
	  { 
		if(l->bg){
			jobsList(pid,l->seq);	
		}else{
		int status;
		waitpid(pid, &status, 0);
		}
		break;
	  }
	}
}

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	printf("Not implemented yet: can not execute %s\n", line);

	/* Remove this line when using parsecmd as it will free it */
	free(line);
	
	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}


int main() {
		/* signal utilisé pour réveiller un processus
		 *  dont un des fils vient de mourir, et invoque la fonction sigHandler.
		*/
		/*signal(SIGCHLD,sigHandler);
		signal(SIGALRM,sigHandler);
		signal(SIGUSR1,sigHandler);*/

        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {

		struct cmdline *l;
		char *line=0;
		int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		

		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");

		/*Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
		}
		if(lProcess == NULL){
			lProcess = malloc(sizeof(struct listProcess));
			lProcess->premier=NULL;
		}

	/*la fonction pipeFunction est appelé lorsque 
	 * la deuxieme sequence de la ligne de commande 
	 * est non null, en traite juste le cas deux de séquence
	 */	
		if(l->seq[1]){
			pipeFunction(l);
		}
		else{
			lireEtExecuter(l);
		}
	}

}
