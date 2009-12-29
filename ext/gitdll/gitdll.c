// gitdll.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "git-compat-util.h"
#include "msvc.h"
#include "gitdll.h"
#include "cache.h"
#include "commit.h"
#include "diff.h"
#include "revision.h"

const char git_version_string[] = GIT_VERSION;

#if 0

// This is an example of an exported variable
GITDLL_API int ngitdll=0;

// This is an example of an exported function.
GITDLL_API int fngitdll(void)
{
	return 42;
}

// This is the constructor of a class that has been exported.
// see gitdll.h for the class definition
Cgitdll::Cgitdll()
{
	return;
}
#endif

#define MAX_ERROR_STR_SIZE 512
char g_last_error[MAX_ERROR_STR_SIZE]={0};
void * g_prefix;

char * get_git_last_error()
{
	return g_last_error;
}

static void die_dll(const char *err, va_list params)
{
	memset(g_last_error,0,MAX_ERROR_STR_SIZE);
	vsnprintf(g_last_error, MAX_ERROR_STR_SIZE-1, err, params);	
}

void dll_entry()
{
	set_die_routine(die_dll);
}

int git_get_sha1(const char *name, GIT_HASH sha1)
{
	return get_sha1(name,sha1);
}

static int convert_slash(char * path)
{
	while(*path)
	{
		if(*path == '\\' )
			*path = '/';
		path++;
	}
}

int git_init()
{
	char *home;
	char path[MAX_PATH+1];
	char *prefix;
	size_t homesize,size,httpsize;

	// set HOME if not set already
	getenv_s(&homesize, NULL, 0, "HOME");
	if (!homesize)
	{
		_dupenv_s(&home,&size,"USERPROFILE"); 
		_putenv_s("HOME",home);
		free(home);
	}
	GetModuleFileName(NULL, path, MAX_PATH);
	convert_slash(path);

	git_extract_argv0_path(path);
	g_prefix = prefix = setup_git_directory();
	return git_config(git_default_config, NULL);
}

static int git_parse_commit_author(struct GIT_COMMIT_AUTHOR *author, char *pbuff)
{
	char *end;

	author->Name=pbuff;
	end=strchr(pbuff,'<');
	if( end == 0)
	{
		return -1;
	}
	author->NameSize = end - pbuff - 1;

	pbuff = end +1;
	end = strchr(pbuff, '>');
	if( end == 0)
		return -1;

	author->Email = pbuff ;
	author->EmailSize = end - pbuff;

	pbuff = end + 2;

	author->Date = atol(pbuff);
	end =  strchr(pbuff, ' ');
	if( end == 0 )
		return -1;

	pbuff=end;
	author->TimeZone = atol(pbuff);

	return 0;
}

int git_parse_commit(GIT_COMMIT *commit)
{
	int ret = 0;
	char *pbuf;
	char *end;
	struct commit *p;

	p= (struct commit *)commit->m_pGitCommit;

	memcpy(commit->m_hash,p->object.sha1,GIT_HASH_SIZE);

	if(p->buffer == NULL)
		return -1;

	pbuf = p->buffer;
	while(pbuf)
	{
		if( strncmp(pbuf,"author",6) == 0)
		{
			ret = git_parse_commit_author(&commit->m_Author,pbuf + 7);
			if(ret)
				return ret;
		}
		if( strncmp(pbuf, "committer",9) == 0)
		{
			ret =  git_parse_commit_author(&commit->m_Committer,pbuf + 10);
			if(ret)
				return ret;

			pbuf = strchr(pbuf,'\n');
			if(pbuf == NULL)
				return -1;

			while((*pbuf) && (*pbuf == '\n'))
				pbuf ++;

			commit->m_Subject=pbuf;
			end = strchr(pbuf,'\n');
			if( end == 0)
				commit->m_SubjectSize = strlen(pbuf);
			else
			{
				commit->m_SubjectSize = end - pbuf;
				pbuf = end +1;
				commit->m_Body = pbuf;
				commit->m_BodySize = strlen(pbuf);
				return 0;
			}

		}

		pbuf = strchr(pbuf,'\n');
		if(pbuf)
			pbuf ++;
	}

}

int git_get_commit_from_hash(GIT_COMMIT *commit, GIT_HASH hash)
{
	int ret = 0;
	
	struct commit *p;
	commit->m_pGitCommit = p = lookup_commit(hash);

	if(commit == NULL)
		return -1;
	
	if(p == NULL)
		return -1;
	
	ret = parse_commit(p);
	if( ret )
		return ret;

	return git_parse_commit(commit);
}

int git_free_commit(GIT_COMMIT *commit)
{
	struct commit *p = commit->m_pGitCommit;

	if( p->parents)
		free_commit_list(p->parents);	

	if( p->buffer )
	{
		free(p->buffer);
	}
	memset(commit,0,sizeof(GIT_COMMIT));
	return 0;
}

int git_get_diff(GIT_COMMIT *commit, GIT_DIFF *diff)
{

}

char **strtoargv(char *arg, int *size)
{
	int count=0;
	char *p=arg;
	char **argv;
	int i=0;
	while(*p)
	{
		if(*p == ' ')
			count ++;
		p++;
	}
	
	argv=malloc(strlen(arg)+1 + (count +2)*sizeof(void*));
	p=(char*)(argv+count+2);

	while(*arg)
	{
		if(*arg == '"')
		{
			argv[i] = p;
			arg++;
			*p=*arg;
			while(*arg && *arg!= '"')
				*p++=*arg++;
			*p++=0;
			arg++;
			i++;
			if(*arg == 0)
				break;
		}
		if(*arg != ' ')
		{
			argv[i]=p;
			while(*arg && *arg !=' ')
				*p++ = *arg++;
			i++;
			*p++=0;
		}
		arg++;
	}
	argv[i]=NULL;
	*size = i;
	return argv;
}
int git_open_log(GIT_LOG * handle, char * arg)
{
	struct rev_info *p_Rev;
	int size;
	char ** argv=0;
	int argc=0;
	
	if(arg != NULL)
		argv = strtoargv(arg,&argc);

	p_Rev = malloc(sizeof(struct rev_info));
	memset(p_Rev,0,sizeof(struct rev_info));

	if(p_Rev == NULL)
		return -1;

	init_revisions(p_Rev, g_prefix);
	p_Rev->diff = 1;
	p_Rev->simplify_history = 0;
	
	cmd_log_init(argc, argv, g_prefix,p_Rev);

	p_Rev->pPrivate = argv;
	*handle = p_Rev;
	return 0;

}
int git_get_log_firstcommit(GIT_LOG handle)
{
	return prepare_revision_walk(handle);
}

int git_get_log_nextcommit(GIT_LOG handle, GIT_COMMIT *commit)
{
	int ret =0;

	commit->m_pGitCommit = get_revision(handle);
	if( commit->m_pGitCommit == NULL)
		return -2;
	
	ret=git_parse_commit(commit);
	if(ret)
		return ret;

	return 0;
}

int git_close_log(GIT_LOG handle)
{
	if(handle)
	{
		struct rev_info *p_Rev;
		p_Rev=(struct rev_info *)handle;
		if(p_Rev->pPrivate)
			free(p_Rev->pPrivate);
		free(handle);
	}
	
	return 0;
}