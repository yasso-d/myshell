#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>
#include<fcntl.h>

//实现基础数据结构和工具函数





//归档文件函数
int tar_file(char *filename, char *tarname)
{
	int fd = open(tarname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(fd == -1)
	{
		perror("open");
		return -1;
	}
	
	int src_fd = open(filename, O_RDONLY);
	if(src_fd == -1)
	{
		perror("open");
		close(fd);
		return -1;
	}
	
	char buffer[1024];
	int len;
	while((len = read(src_fd, buffer, sizeof(buffer))) > 0)
	{
		write(fd, buffer, len);
	}
	
	close(src_fd);
	close(fd);
	return 0;
}

int main()
{
	return 0；
}
