#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <unistd.h>  // getpid(), fork()
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>  // pid_t

#define MSGSZ 255

typedef struct msgbuf {
    long    mtype;
    char    mtext[MSGSZ];
} message_buf;

void remove_spaces(char **str)
{
    char *temp;
    temp=*str;
    char c[strlen(*str) + 1];
    int i = 0;
    for( ; *temp != '\0'; ++temp)
        if (!isspace(*temp)) {
            c[i] = *temp;
            i++;
        }
    *str=NULL;
    *str = (char*)malloc(i++*sizeof(char));
    strncpy(*str, c, i*sizeof(char));
    // printf("rs %s\n", *str);
}


int calculate_expression(char* source)
{
    float x1, x2;
    char *operator_type;
    operator_type = (char*) malloc(sizeof(char));
    char* pattern = "(\\-?[0-9]+)([\\+-\\*/]{1})(\\-?[0-9]+)";
    size_t maxMatches = 1;
    size_t maxGroups = 4;

    regex_t regexCompiled;
    regmatch_t groupArray[maxGroups];
    unsigned int m;
    char* cursor;

    if (regcomp(&regexCompiled, pattern, REG_EXTENDED))
    {
        printf("Could not compile regular expression.\n");
        return 1;
    };

    m = 0;
    cursor = source;
    for (m = 0; m < maxMatches; m++)
    {
        if (regexec(&regexCompiled, cursor, maxGroups, groupArray, 0)) {
            printf("Nah\n");
            break;  // No more matches
        }
        // puts("jam");
        unsigned int g = 0;
        unsigned int offset = 0;
        for (g = 0; g < maxGroups; g++)
        {
            // puts("yeah");
            if (groupArray[g].rm_so == (size_t)-1) {
                printf("Yo\n");
                break;  // No more groups
            }

            if (g == 0)
                offset = groupArray[g].rm_eo;

            char cursorCopy[strlen(cursor) + 1];
            strcpy(cursorCopy, cursor);
            cursorCopy[groupArray[g].rm_eo] = 0;
            // printf("Match %u, Group %u: [%2u-%2u]: %s\n",
            //              m, g, groupArray[g].rm_so, groupArray[g].rm_eo,
            //              cursorCopy + groupArray[g].rm_so);
            // printf("%s\n", cursorCopy + groupArray[g].rm_so);
            if (g == 1) x1 = atof(cursorCopy + groupArray[g].rm_so);
            else if (g == 2) strcpy(operator_type, cursorCopy + groupArray[g].rm_so);
            else if (g == 3) x2 = atof(cursorCopy + groupArray[g].rm_so);
            // printf("%f %s %f\n", x1, operator_type, x2);
        }
        cursor += offset;
    }

    regfree(&regexCompiled);
    // printf("Reach here?\n");
    if (strcmp(operator_type, "+") == 0) printf("%f\n", x1+x2);
    else if (strcmp(operator_type, "-") == 0) printf("%f\n", x1-x2);
    else if (strcmp(operator_type, "*") == 0) printf("%f\n", x1*x2);
    else if (strcmp(operator_type, "/") == 0) printf("%f\n", x1/x2);
    free(operator_type);
    return 0;
}


int main() {
    printf("This is main process which has pid is %d\n", getpid());
    /*
     * Get the message queue id for the
     * "name" 1234, which was created by
     * the server.
     */
    key_t KEY = 1234;
    char FINISHED_MESSAGE[17] = "Sending finished";
    pid_t child1_process = fork();
    if (child1_process >= 0) {
        // IN CHILD 1 PROCESS
        if (child1_process == 0) {
            int msqid;
            int msgflg = IPC_CREAT | 0666;
            message_buf sbuf;
            size_t buf_length;

            (void) fprintf(stderr, "msgget: Calling msgget(%d,%#o)\n", KEY, msgflg);

            if ((msqid = msgget(KEY, msgflg)) < 0) {
                perror("msgget");
                exit(1);
            }
            else
                (void) fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);

            // We'll send message type 1
            sbuf.mtype = 1;

            FILE *fp;
            char buff[255], temp[255];
            (void) strcpy(temp, "");
            fp = fopen("/tmp/test.txt", "r");
            while (1 == 1) {
                fgets(buff, 255, (FILE*)fp);
                if (strcmp(temp, buff) == 0) {
                    fclose(fp);
                    (void) strcpy(sbuf.mtext, FINISHED_MESSAGE);
                }
                else {
                    (void) strcpy(temp, buff);
                    (void) strcpy(sbuf.mtext, buff);
                    if (sbuf.mtext[strlen(sbuf.mtext)-1] == '\n')
                        sbuf.mtext[strlen(sbuf.mtext)-1] = '\0';  // Remove \n character
                }
                // Send a message.
                buf_length = strlen(sbuf.mtext) + 1;
                if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {
                    printf ("%d, %ld, %s, %lu\n", msqid, sbuf.mtype, sbuf.mtext, buf_length);
                    perror("msgsnd");
                    exit(1);
                }
                else {
                    printf("Child 1 Message: \"%s\" Sent\n", sbuf.mtext);
                    if (strcmp(sbuf.mtext, FINISHED_MESSAGE) == 0) break;
                }
            }
        }
        // IN MAIN PROCESS
        else {
            pid_t child2_process = fork();
            if (child2_process >= 0) {
                // IN CHILD 2 PROCESS
                if (child2_process == 0) {
                    int msqid;
                    message_buf  rbuf;
                    char *source;

                    printf("Child 2 pid = %d\n", getpid());
                    if ((msqid = msgget(KEY, 0666)) < 0) {
                        perror("msgget");
                        exit(1);
                    }
                    while (1 == 1) {
                        // Receive an answer of message type 1.
                        if (msgrcv(msqid, &rbuf, MSGSZ, 1, 0) < 0) {
                            exit(1);
                        }

                        //Print the answer.
                        if (strcmp(rbuf.mtext, FINISHED_MESSAGE) == 0) break;
                        source = (char*)malloc(strlen(rbuf.mtext)*sizeof(char));  // remember free source
                        strcpy(source, rbuf.mtext);
                        // printf("Child 2 rbuf %s, len = %lu\n", rbuf.mtext, strlen(rbuf.mtext));
                        remove_spaces(&source);
                        // printf("Child 2 source %s, len = %lu\n", source, strlen(source));
                        calculate_expression(source);
                        free(source);
                    }
                }
                else {
                    sleep(3);
                }
            }
        }
    }
    else {
        printf("Error occured in forking.\n");
    }
}
