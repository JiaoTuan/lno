# include<stdio.h>   
# include<sys/types.h>   
# include<unistd.h>
# include<stdlib.h>
# include<sys/wait.h>
int main()
{
	int id;
	if ((id = fork()) == 0) {
		printf("I am the son of you\n");
		return 0;
	}
	else if (id < 0) {
		printf("Son process fails to create\n");
		return 0;
	}
	else if (id > 0) {
		if ((id = fork()) == 0) {
			printf("I am the daughter of you\n");
			return 0;
		}
		else if(id < 0){
			printf("Daughter process fails to cerate\n");
			return 0;
		}
		printf("I am the father of you\n");
		sleep(1);
	}
	return 0;
}