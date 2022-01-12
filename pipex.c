/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   pipex.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sdonny <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/01/10 16:06:53 by sdonny            #+#    #+#             */
/*   Updated: 2022/01/12 17:59:47 by sdonny           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./includes/pipex.h"

#include <stdio.h>

static void	check_infile(char *infile)
{
	if (access(infile, F_OK) != 0)
	{
		perror(infile);
		exit(1);
	}
	if (access(infile, R_OK) != 0)
	{
		perror(infile);
		exit(1);
	}
}

static void	check_outfile(char *outfile)
{
	if (access(outfile, F_OK) != 0)
		return ;
	if (access(outfile, W_OK) != 0)
	{
		perror(outfile);
		exit(1);
	}
}

static void	close_fd(int quantity, int proc, int **fd)
{
	int	i;
	int	j;

	i = -1;
	while (++i < quantity)
	{
		j = -1;
		while (++j < 2)
		{
			if ((i == proc && j == 0) || (i == proc + 1 && j == 1)
				|| (((i == quantity - 1 && j == 0) || (i == 0 && j == 1))
					&& proc == -1))
				continue ;
			close(fd[i][j]);
		}
	}
	if (proc != -1)
	{
		dup2(fd[proc][0], 0);
		close(fd[proc][0]);
		dup2(fd[proc + 1][1], 1);
		close(fd[proc + 1][1]);
	}
}

static int	validate(int argc, char **argv)
{
	check_outfile(argv[argc - 1]);
	if (ft_strncmp("here_doc", argv[1], 9) == 0)
		return (1);
	check_infile(argv[1]);
	return (0);
}

static void	exitmalloc(int quantity, int **fd)
{
	int	i;

	i = -1;
	perror("pipes\n");
	if (fd)
	{
		while (++i < quantity)
			free(fd[i]);
		free(fd);
		fd = NULL;
	}
	exit(1);
}

static int	**multipipe(int m)
{
	int	i;
	int	**fd;

	fd = (int **)malloc(sizeof(int *) * m);
	if (!fd)
		exitmalloc(0, fd);
	i = -1;
	while (++i < m)
	{
		fd[i] = (int *)malloc(sizeof(int) * 2);
		if (!fd[i])
			exitmalloc(i, fd);
		if (pipe(fd[i]) == -1)
			exitmalloc(i, fd);
	}
	return (fd);
}

static void	exitpid(int **fd, pid_t *pid, int quantity, char *desc)
{
	int	i;

	i = -1;
	perror(desc);
	while (++i < quantity + 1)
		free(fd[i]);
	free(fd);
	fd = NULL;
	free(pid);
	pid = NULL;
	exit(1);
}

static void	cleansplit(char **cmd)
{
	int	i;

	i = -1;
	while (cmd[++i])
		free(cmd[i]);
	free(cmd);
	cmd = NULL;
}

static char	*checkpath(char *tmp, char **envp)
{
	char	**paths;
	char	*path;
	char	*slesh;
	int		i;

	i = -1;
	while(ft_strnstr(envp[++i], "PATH", 4) == 0)
		;
	paths = ft_split(envp[i] + 5, ':');
	i = -1;
	while (paths[++i])
	{
		slesh = ft_strjoin(paths[i], "/");
		path = ft_strjoin(slesh, tmp);
		free(slesh);
		if (access(path, F_OK) == 0)
		{
			cleansplit(paths);
			return (path);
		}
		free(path);
	}
	cleansplit(paths);
	return (NULL);
}

static char	**cmdparse(char *old, char **envp)
{
	char	**new;
	char	*tmp;

	new = ft_split(old, ' ');
	tmp = new[0];
	new[0] = checkpath(tmp, envp);
	free(tmp);
	tmp = NULL;
	return (new);
}

static pid_t	*forks(int **fd, int quantity, char	**argv, char **envp)
{
	int		m;
	pid_t	*pid;
	char	**cmd;

	m = -1;
	pid = (pid_t *)malloc(sizeof(pid_t) * quantity);
	if (!pid)
		exitmalloc(quantity, fd);
	while (++m < quantity)
	{
		pid[m] = fork();
		if (pid[m] < 0)
			exitpid(fd, pid, quantity,"fork");
		if (!pid[m])
		{
			close_fd(quantity + 1, m, fd);
			cmd = cmdparse(argv[m + 2 + 1], envp);
			if (execve(cmd[0], cmd, envp) == -1)
				exitpid(fd, pid, quantity, "execve");
			cleansplit(cmd);
			close(0);
			close(1);
			exit(0);
		}
	}
	return (pid);
}

static int	parentread(char *cmp, int fd, char *filename)
{
	char	*buf;
	int		file;

	if (!cmp)
		file = open(filename, O_RDONLY);
	else
		file = 0;
	if (file == -1)
		return (-1);
	while (1)
	{
		buf = get_next_line(file);
		if ((cmp && ft_strncmp(buf, cmp, ft_strlen(cmp)) == 0)
			|| (!buf && !cmp))
			break ;
		ft_putstr_fd(buf, fd);
		free(buf);
	}
	close(fd);
	if (!cmp)
		close(file);
	return (0);
}

static int	parentwrite(int fd, char *filename, int flag)
{
	char	*buf;
	int		file;

	if (!flag)
		file = open(filename, O_WRONLY | O_CREAT, 0664);
	else
		file = open(filename, O_APPEND | O_CREAT, 0664);
	if (file == -1)
		return (-1);
	while (1)
	{
		buf = get_next_line(fd);
		ft_putstr_fd(buf, file);
		if (!buf)
			break ;
		free(buf);
	}
	close(fd);
	close(file);
	return (0);
}

static void	waitchildren(pid_t *pid, int **fd, int argc)
{
	int	m;

	m = -1;
	while (++m < argc)
	{
		waitpid(pid[m], NULL, 0);
		free(fd[m]);
	}
	free(fd[m]);
	free(fd);
	free(pid);
}

int	main(int argc, char **argv, char **envp)
{
	int		**fd;
	int		m;
	int		check;
	pid_t	*pid;

	if (argc < 4)
	{
		ft_putstr_fd("Error!\nToo few arguments!\n", 2);
		exit(1);
	}
	m = validate(argc, argv);
	fd = multipipe(argc - 2 - m);
	pid = forks(fd, argc - 3 - m, argv, envp);
	close_fd(argc - 2 - m, -1, fd);
	if (!m)
		check = parentread(NULL, fd[0][1], argv[1]);
	else
		check = parentread(argv[2], fd[0][1], NULL);
	if (check == -1)
		exitpid(fd, pid, argc, "parentread");
	check = parentwrite(fd[argc - 3 - m][0], argv[argc - 1], m);
	if (check == -1)
		exitpid(fd, pid, argc, "parentwrite");
	waitchildren(pid, fd, argc - 3 - m);
}
