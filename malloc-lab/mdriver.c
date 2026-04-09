/*
 * mdriver.c - CS:APP Malloc Lab Driver
 * mdriver.c - CS:APP Malloc Lab 드라이버입니다.
 *
 * Uses a collection of trace files to tests a malloc/free/realloc
 * 여러 추적 파일 모음을 사용해 malloc/free/realloc
 * implementation in mm.c.
 * 구현이 mm.c에서 올바르게 동작하는지 테스트합니다.
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * 저작권 (c) 2002, R. Bryant와 D. O'Hallaron, 모든 권리 보유.
 * May not be used, modified, or copied without permission.
 * 허가 없이 사용, 수정, 복사할 수 없습니다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <time.h>

extern char *optarg; // Added declaration for optarg // optarg에 대한 선언 추가

#include "mm.h"
#include "memlib.h"
#include "fsecs.h"
#include "config.h"

/**********************
 * Constants and macros
 * 상수와 매크로
 **********************/

/* Misc */
/* 기타 */
#define MAXLINE 1024	   /* max string size */ /* 최대 문자열 크기 */
#define HDRLINES 4		   /* number of header lines in a trace file */ /* 추적 파일의 헤더 줄 수 */
#define LINENUM(i) (i + 5) /* cnvt trace request nums to linenums (origin 1) */ /* 추적 요청 번호를 줄 번호로 변환합니다 (시작 값 1) */

/* Returns true if p is ALIGNMENT-byte aligned */
/* p가 ALIGNMENT 바이트 경계에 정렬되어 있으면 참을 반환합니다 */
#define IS_ALIGNED(p) ((((unsigned int)(p)) % ALIGNMENT) == 0)

/******************************
 * The key compound data types
 * 주요 복합 데이터 타입
 *****************************/

/* Records the extent of each block's payload */
/* 각 블록의 페이로드 크기를 기록합니다 */
typedef struct range_t
{
	char *lo;			  /* low payload address */ /* 낮은 페이로드 주소 */
	char *hi;			  /* high payload address */ /* 높은 페이로드 주소 */
	struct range_t *next; /* next list element */ /* 다음 목록 항목 */
} range_t;

/* Characterizes a single trace operation (allocator request) */
/* 단일 추적 작업(할당기 요청)을 나타냅니다 */
typedef struct
{
	enum
	{
		ALLOC,
		FREE,
		REALLOC
	} type;	   /* type of request */ /* 요청 유형 */
	int index; /* index for free() to use later */ /* 나중에 free() 함수가 사용할 인덱스 */
	int size;  /* byte size of alloc/realloc request */ /* 할당/재할당 요청의 바이트 크기 */
} traceop_t;

/* Holds the information for one trace file*/
/* 하나의 추적 파일에 대한 정보를 저장합니다 */
typedef struct
{
	int sugg_heapsize;	 /* suggested heap size (unused) */ /* 권장 힙 크기 (미사용) */
	int num_ids;		 /* number of alloc/realloc ids */ /* 할당/재할당 ID 수 */
	int num_ops;		 /* number of distinct requests */ /* 고유 요청 수 */
	int weight;			 /* weight for this trace (unused) */ /* 이 추적에 대한 가중치 (사용되지 않음) */
	traceop_t *ops;		 /* array of requests */ /* 요청 배열 */
	char **blocks;		 /* array of ptrs returned by malloc/realloc... */ /* malloc/realloc 등이 반환하는 포인터 배열 */
	size_t *block_sizes; /* ... and a corresponding array of payload sizes */ /* ... 그리고 이에 대응하는 페이로드 크기 배열 */
} trace_t;

/*
 * Holds the params to the xxx_speed functions, which are timed by fcyc.
 * This struct is necessary because fcyc accepts only a pointer array
 * as input.
 * fcyc에 의해 타이밍이 조절되는 xxx_speed 함수들의 매개변수를 저장합니다.
 * fcyc는 입력으로 포인터 배열만 허용하기 때문에 이 구조체가 필요합니다.
 */
typedef struct
{
	trace_t *trace;
	range_t *ranges;
} speed_t;

/* Summarizes the important stats for some malloc function on some trace */
/* 특정 추적에서 어떤 malloc 함수의 중요한 통계를 요약합니다 */
typedef struct
{
	/* defined for both libc malloc and student malloc package (mm.c) */
	/* libc malloc과 학생 malloc 패키지(mm.c) 모두에 대해 정의됩니다 */
	double ops;	 /* number of ops (malloc/free/realloc) in the trace */ /* 추적 내 연산 수 (malloc/free/realloc) */
	int valid;	 /* was the trace processed correctly by the allocator? */ /* 할당기가 이 추적을 올바르게 처리했는가? */
 	double secs; /* number of secs needed to run the trace */ /* 추적 실행에 걸린 시간(초) */
 
	/* defined only for the student malloc package */
	/* 학생 malloc 패키지에 대해서만 정의됩니다 */
	double util; /* space utilization for this trace (always 0 for libc) */ /* 이 추적의 공간 활용도 (libc의 경우 항상 0) */

	/* Note: secs and util are only defined if valid is true */
	/* 참고: secs와 util은 valid가 참일 때만 정의됩니다 */
} stats_t;

/********************
 * Global variables
 * 전역 변수
 *******************/
int verbose = 0;	   /* global flag for verbose output */ /* 자세한 출력을 위한 전역 플래그 */
static int errors = 0; /* number of errs found when running student malloc */ /* 학생 malloc 실행 중 발견된 오류 수 */
char msg[MAXLINE];	   /* for whenever we need to compose an error message */ /* 오류 메시지를 조합할 때 사용하는 버퍼 */

/* Directory where default tracefiles are found */
/* 기본 추적 파일을 찾는 디렉터리 */
static char tracedir[MAXLINE] = TRACEDIR;

/* The filenames of the default tracefiles */
/* 기본 추적 파일 이름 목록 */
static char *default_tracefiles[] = {
	DEFAULT_TRACEFILES, NULL};

/*********************
 * Function prototypes
 * 함수 원형
 *********************/

/* these functions manipulate range lists */
/* 이 함수들은 범위 목록을 조작합니다 */
static int add_range(range_t **ranges, char *lo, int size,
					 int tracenum, int opnum);
static void remove_range(range_t **ranges, char *lo);
static void clear_ranges(range_t **ranges);

/* These functions read, allocate, and free storage for traces */
/* 이 함수들은 추적 정보를 읽고, 메모리를 할당하고, 해제합니다 */
static trace_t *read_trace(char *tracedir, char *filename);
static void free_trace(trace_t *trace);

/* Routines for evaluating the correctness and speed of libc malloc */
/* libc malloc의 정확성과 속도를 평가하는 루틴입니다 */
static int eval_libc_valid(trace_t *trace, int tracenum);
static void eval_libc_speed(void *ptr);

/* Routines for evaluating correctnes, space utilization, and speed
   of the student's malloc package in mm.c */
/* mm.c에 있는 학생 malloc 패키지의 정확성, 공간 활용도, 속도를 평가하는 루틴입니다 */
static int eval_mm_valid(trace_t *trace, int tracenum, range_t **ranges);
static double eval_mm_util(trace_t *trace, int tracenum, range_t **ranges);
static void eval_mm_speed(void *ptr);

/* Various helper routines */
/* 여러 보조 루틴입니다 */
static void printresults(int n, stats_t *stats);
static void usage(void);
static void unix_error(char *msg);
static void malloc_error(int tracenum, int opnum, char *msg);
static void app_error(char *msg);

/**************
 * Main routine
 * 메인 루틴
 **************/
int main(int argc, char **argv)
{
	int i;
	int c;
	char **tracefiles = NULL;	/* null-terminated array of trace file names */ /* NULL로 끝나는 추적 파일 이름 배열 */
	int num_tracefiles = 0;		/* the number of traces in that array */ /* 그 배열에 있는 추적 파일 수 */
	trace_t *trace = NULL;		/* stores a single trace file in memory */ /* 단일 추적 파일을 메모리에 저장합니다 */
	range_t *ranges = NULL;		/* keeps track of block extents for one trace */ /* 하나의 추적에 대한 블록 범위를 추적합니다 */
	stats_t *libc_stats = NULL; /* libc stats for each trace */ /* 각 추적에 대한 libc 통계 */
	stats_t *mm_stats = NULL;	/* mm (i.e. student) stats for each trace */ /* 각 추적에 대한 mm(학생) 통계 */
	speed_t speed_params;		/* input parameters to the xx_speed routines */ /* xx_speed 루틴의 입력 매개변수 */

	int team_check = 1; /* If set, check team structure (reset by -a) */ /* 설정되면 팀 구조를 검사합니다 (-a로 해제) */
	int run_libc = 0;	/* If set, run libc malloc (set by -l) */ /* 설정되면 libc malloc을 실행합니다 (-l로 설정) */
	int autograder = 0; /* If set, emit summary info for autograder (-g) */ /* 설정되면 자동 채점기용 요약 정보를 출력합니다 (-g) */

	/* temporaries used to compute the performance index */
	/* 성능 지수 계산에 사용하는 임시 변수들 */
	double secs, ops, util, avg_mm_util, avg_mm_throughput, p1, p2, perfindex;
	int numcorrect;

	/*
	 * Read and interpret the command line arguments
	 * 명령줄 인수를 읽고 해석합니다
	 */
	while ((c = getopt(argc, argv, "f:t:hvVgal")) != EOF)
	{
		printf("getopt returned: %d\n", c); // 디버깅용 출력 추가

		switch (c)
		{
		case 'g': /* Generate summary info for the autograder */ /* 자동 채점기를 위한 요약 정보를 생성합니다 */
			autograder = 1;
			break;
		case 'f': /* Use one specific trace file only (relative to curr dir) */ /* 특정 추적 파일 하나만 사용합니다 (현재 디렉터리 기준 상대 경로) */
			num_tracefiles = 1;
			if ((tracefiles = realloc(tracefiles, 2 * sizeof(char *))) == NULL)
				unix_error("ERROR: realloc failed in main");
			strcpy(tracedir, "./");
			tracefiles[0] = strdup(optarg);
			tracefiles[1] = NULL;
			break;
		case 't':					 /* Directory where the traces are located */ /* 추적 파일이 위치한 디렉터리 */
			if (num_tracefiles == 1) /* ignore if -f already encountered */ /* 이미 -f가 주어진 경우 무시합니다 */
				break;
			strcpy(tracedir, optarg);
			if (tracedir[strlen(tracedir) - 1] != '/')
				strcat(tracedir, "/"); /* path always ends with "/" */ /* 경로는 항상 "/"로 끝나야 합니다 */
			break;
		case 'a': /* Don't check team structure */ /* 팀 구조를 검사하지 않습니다 */
			team_check = 0;
			break;
		case 'l': /* Run libc malloc */ /* libc malloc도 실행합니다 */
			run_libc = 1;
			break;
		case 'v': /* Print per-trace performance breakdown */ /* 추적별 성능 세부 정보를 출력합니다 */
			verbose = 1;
			break;
		case 'V': /* Be more verbose than -v */ /* -v보다 더 자세히 출력합니다 */
			verbose = 2;
			break;
		case 'h': /* Print this message */ /* 이 도움말 메시지를 출력합니다 */
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	/*
	 * Check and print team info
	 * 팀 정보를 확인하고 출력합니다
	 */
	if (team_check)
	{
		/* Students must fill in their team information */
		/* 학생들은 팀 정보를 반드시 채워야 합니다 */
		if (!strcmp(team.teamname, ""))
		{
			printf("ERROR: Please provide the information about your team in mm.c.\n");
			exit(1);
		}
		else
			printf("Team Name:%s\n", team.teamname);
		if ((*team.name1 == '\0') || (*team.id1 == '\0'))
		{
			printf("ERROR.  You must fill in all team member 1 fields!\n");
			exit(1);
		}
		else
			printf("Member 1 :%s:%s\n", team.name1, team.id1);

		if (((*team.name2 != '\0') && (*team.id2 == '\0')) ||
			((*team.name2 == '\0') && (*team.id2 != '\0')))
		{
			printf("ERROR.  You must fill in all or none of the team member 2 ID fields!\n");
			exit(1);
		}
		else if (*team.name2 != '\0')
			printf("Member 2 :%s:%s\n", team.name2, team.id2);
	}

	/*
	 * If no -f command line arg, then use the entire set of tracefiles
	 * defined in default_traces[]
	 * -f 명령줄 인수가 없으면 default_traces[]에 정의된 모든 추적 파일 세트를 사용합니다.
	 */
	if (tracefiles == NULL)
	{
		tracefiles = default_tracefiles;
		num_tracefiles = sizeof(default_tracefiles) / sizeof(char *) - 1;
		printf("Using default tracefiles in %s\n", tracedir);
	}

	/* Initialize the timing package */
	/* 타이밍 패키지를 초기화합니다 */
	init_fsecs();

	/*
	 * Optionally run and evaluate the libc malloc package
	 * 필요에 따라 libc malloc 패키지를 실행하고 평가합니다
	 */
	if (run_libc)
	{
		if (verbose > 1)
			printf("\nTesting libc malloc\n");

		/* Allocate libc stats array, with one stats_t struct per tracefile */
		/* 추적 파일마다 stats_t 구조체 하나를 갖는 libc stats 배열을 할당합니다 */
		libc_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
		if (libc_stats == NULL)
			unix_error("libc_stats calloc in main failed");

		/* Evaluate the libc malloc package using the K-best scheme */
		/* K-best 방식으로 libc malloc 패키지를 평가합니다 */
		for (i = 0; i < num_tracefiles; i++)
		{
			trace = read_trace(tracedir, tracefiles[i]);
			libc_stats[i].ops = trace->num_ops;
			if (verbose > 1)
				printf("Checking libc malloc for correctness, ");
			libc_stats[i].valid = eval_libc_valid(trace, i);
			if (libc_stats[i].valid)
			{
				speed_params.trace = trace;
				if (verbose > 1)
					printf("and performance.\n");
				libc_stats[i].secs = fsecs(eval_libc_speed, &speed_params);
			}
			free_trace(trace);
		}

		/* Display the libc results in a compact table */
		/* libc 결과를 간결한 표 형태로 출력합니다 */
		if (verbose)
		{
			printf("\nResults for libc malloc:\n");
			printresults(num_tracefiles, libc_stats);
		}
	}

	/*
	 * Always run and evaluate the student's mm package
	 * 학생의 mm 패키지는 항상 실행하고 평가합니다
	 */
	if (verbose > 1)
		printf("\nTesting mm malloc\n");

	/* Allocate the mm stats array, with one stats_t struct per tracefile */
	/* 트레이스 파일당 stats_t 구조체 하나씩을 포함하도록 mm stats 배열을 할당합니다 */
	mm_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
	if (mm_stats == NULL)
		unix_error("mm_stats calloc in main failed");

	/* Initialize the simulated memory system in memlib.c */
	/* memlib.c의 시뮬레이션 메모리 시스템을 초기화합니다 */
	mem_init();

	/* Evaluate student's mm malloc package using the K-best scheme */
	/* K-best 방식으로 학생의 mm malloc 패키지를 평가합니다 */
	for (i = 0; i < num_tracefiles; i++)
	{
		trace = read_trace(tracedir, tracefiles[i]);
		mm_stats[i].ops = trace->num_ops;
		if (verbose > 1)
			printf("Checking mm_malloc for correctness, ");
		mm_stats[i].valid = eval_mm_valid(trace, i, &ranges);
		if (mm_stats[i].valid)
		{
			if (verbose > 1)
				printf("efficiency, ");
			mm_stats[i].util = eval_mm_util(trace, i, &ranges);
			speed_params.trace = trace;
			speed_params.ranges = ranges;
			if (verbose > 1)
				printf("and performance.\n");
			mm_stats[i].secs = fsecs(eval_mm_speed, &speed_params);
		}
		free_trace(trace);
	}

	/* Display the mm results in a compact table */
	/* mm 결과를 간결한 표 형태로 출력합니다 */
	if (verbose)
	{
		printf("\nResults for mm malloc:\n");
		printresults(num_tracefiles, mm_stats);
		printf("\n");
	}

	/*
	 * Accumulate the aggregate statistics for the student's mm package
	 * 학생의 mm 패키지에 대한 종합 통계를 누적합니다
	 */
	secs = 0;
	ops = 0;
	util = 0;
	numcorrect = 0;
	for (i = 0; i < num_tracefiles; i++)
	{
		secs += mm_stats[i].secs;
		ops += mm_stats[i].ops;
		util += mm_stats[i].util;
		if (mm_stats[i].valid)
			numcorrect++;
	}
	avg_mm_util = util / num_tracefiles;

	/*
	 * Compute and print the performance index
	 * 성능 지수를 계산하고 출력합니다
	 */
	if (errors == 0)
	{
		avg_mm_throughput = ops / secs;

		p1 = UTIL_WEIGHT * avg_mm_util;
		if (avg_mm_throughput > AVG_LIBC_THRUPUT)
		{
			p2 = (double)(1.0 - UTIL_WEIGHT);
		}
		else
		{
			p2 = ((double)(1.0 - UTIL_WEIGHT)) *
				 (avg_mm_throughput / AVG_LIBC_THRUPUT);
		}

		perfindex = (p1 + p2) * 100.0;
		printf("Perf index = %.0f (util) + %.0f (thru) = %.0f/100\n",
			   p1 * 100,
			   p2 * 100,
			   perfindex);
	}
	else
	{ /* There were errors */ /* 오류가 발생했습니다 */
		perfindex = 0.0;
		printf("Terminated with %d errors\n", errors);
	}

	if (autograder)
	{
		printf("correct:%d\n", numcorrect);
		printf("perfidx:%.0f\n", perfindex);
	}

	exit(0);
}

/*****************************************************************
 * The following routines manipulate the range list, which keeps
 * 아래 루틴들은 범위 목록을 조작합니다. 이 목록은
 * track of the extent of every allocated block payload. We use the
 * 할당된 각 블록의 페이로드 범위를 추적합니다. 우리는
 * range list to detect any overlapping allocated blocks.
 * 이 범위 목록을 사용해 서로 겹치는 할당 블록을 탐지합니다.
 ****************************************************************/

/*
 * add_range - As directed by request opnum in trace tracenum,
 * add_range - tracenum 추적의 opnum 요청에 따라
 *     we've just called the student's mm_malloc to allocate a block of
 *     학생의 mm_malloc을 호출해
 *     size bytes at addr lo. After checking the block for correctness,
 *     주소 lo에 size 바이트 블록을 할당했습니다. 블록의 정확성을 검사한 뒤,
 *     we create a range struct for this block and add it to the range list.
 *     이 블록에 대한 range 구조체를 만들고 범위 목록에 추가합니다.
 */
static int add_range(range_t **ranges, char *lo, int size,
					 int tracenum, int opnum)
{
	char *hi = lo + size - 1;
	range_t *p;
	char msg[MAXLINE];

	assert(size > 0);

	/* Payload addresses must be ALIGNMENT-byte aligned */
	/* 페이로드 주소는 ALIGNMENT 바이트 경계에 정렬되어야 합니다 */
	if (!IS_ALIGNED(lo))
	{
		sprintf(msg, "Payload address (%p) not aligned to %d bytes",
				lo, ALIGNMENT);
		malloc_error(tracenum, opnum, msg);
		return 0;
	}

	/* The payload must lie within the extent of the heap */
	/* 페이로드는 힙 범위 안에 있어야 합니다 */
	if ((lo < (char *)mem_heap_lo()) || (lo > (char *)mem_heap_hi()) ||
		(hi < (char *)mem_heap_lo()) || (hi > (char *)mem_heap_hi()))
	{
		sprintf(msg, "Payload (%p:%p) lies outside heap (%p:%p)",
				lo, hi, mem_heap_lo(), mem_heap_hi());
		malloc_error(tracenum, opnum, msg);
		return 0;
	}

	/* The payload must not overlap any other payloads */
	/* 페이로드는 다른 어떤 페이로드와도 겹치면 안 됩니다 */
	for (p = *ranges; p != NULL; p = p->next)
	{
		if ((lo >= p->lo && lo <= p->hi) ||
			(hi >= p->lo && hi <= p->hi))
		{
			sprintf(msg, "Payload (%p:%p) overlaps another payload (%p:%p)\n",
					lo, hi, p->lo, p->hi);
			malloc_error(tracenum, opnum, msg);
			return 0;
		}
	}

	/*
	 * Everything looks OK, so remember the extent of this block
	 * 모든 것이 정상으로 보이므로 이 블록의 범위를 기억하기 위해
	 * by creating a range struct and adding it the range list.
	 * range 구조체를 만들고 범위 목록에 추가합니다.
	 */
	if ((p = (range_t *)malloc(sizeof(range_t))) == NULL)
		unix_error("malloc error in add_range");
	p->next = *ranges;
	p->lo = lo;
	p->hi = hi;
	*ranges = p;
	return 1;
}

/*
 * remove_range - Free the range record of block whose payload starts at lo
 * remove_range - 페이로드가 lo에서 시작하는 블록의 범위 기록을 해제합니다
 */
static void remove_range(range_t **ranges, char *lo)
{
	range_t *p;
	range_t **prevpp = ranges;
	int size;

	for (p = *ranges; p != NULL; p = p->next)
	{
		if (p->lo == lo)
		{
			*prevpp = p->next;
			size = p->hi - p->lo + 1;
			free(p);
			break;
		}
		prevpp = &(p->next);
	}
}

/*
 * clear_ranges - free all of the range records for a trace
 * clear_ranges - 하나의 추적에 대한 모든 범위 기록을 해제합니다
 */
static void clear_ranges(range_t **ranges)
{
	range_t *p;
	range_t *pnext;

	for (p = *ranges; p != NULL; p = pnext)
	{
		pnext = p->next;
		free(p);
	}
	*ranges = NULL;
}

/**********************************************
 * The following routines manipulate tracefiles
 * 아래 루틴들은 추적 파일을 다룹니다
 *********************************************/

/*
 * read_trace - read a trace file and store it in memory
 * read_trace - 추적 파일을 읽어 메모리에 저장합니다
 */
static trace_t *read_trace(char *tracedir, char *filename)
{
	FILE *tracefile;
	trace_t *trace;
	char type[MAXLINE];
	char path[MAXLINE];
	unsigned index, size;
	unsigned max_index = 0;
	unsigned op_index;

	if (verbose > 1)
		printf("Reading tracefile: %s\n", filename);

	/* Allocate the trace record */
	/* 추적 레코드를 할당합니다 */
	if ((trace = (trace_t *)malloc(sizeof(trace_t))) == NULL)
		unix_error("malloc 1 failed in read_trance");

	/* Read the trace file header */
	/* 추적 파일 헤더를 읽습니다 */
	strcpy(path, tracedir);
	strcat(path, filename);
	if ((tracefile = fopen(path, "r")) == NULL)
	{
		sprintf(msg, "Could not open %s in read_trace", path);
		unix_error(msg);
	}
	fscanf(tracefile, "%d", &(trace->sugg_heapsize)); /* not used */ /* 사용되지 않음 */
	fscanf(tracefile, "%d", &(trace->num_ids));
	fscanf(tracefile, "%d", &(trace->num_ops));
	fscanf(tracefile, "%d", &(trace->weight)); /* not used */ /* 사용되지 않음 */

	/* We'll store each request line in the trace in this array */
	/* 추적 파일의 각 요청 줄을 이 배열에 저장합니다 */
	if ((trace->ops =
			 (traceop_t *)malloc(trace->num_ops * sizeof(traceop_t))) == NULL)
		unix_error("malloc 2 failed in read_trace");

	/* We'll keep an array of pointers to the allocated blocks here... */
	/* 할당된 블록을 가리키는 포인터 배열을 여기에 저장합니다... */
	if ((trace->blocks =
			 (char **)malloc(trace->num_ids * sizeof(char *))) == NULL)
		unix_error("malloc 3 failed in read_trace");

	/* ... along with the corresponding byte sizes of each block */
	/* ... 그리고 각 블록에 대응하는 바이트 크기도 함께 저장합니다 */
	if ((trace->block_sizes =
			 (size_t *)malloc(trace->num_ids * sizeof(size_t))) == NULL)
		unix_error("malloc 4 failed in read_trace");

	/* read every request line in the trace file */
	/* 추적 파일의 모든 요청 줄을 읽습니다 */
	index = 0;
	op_index = 0;
	while (fscanf(tracefile, "%s", type) != EOF)
	{
		switch (type[0])
		{
		case 'a':
			fscanf(tracefile, "%u %u", &index, &size);
			trace->ops[op_index].type = ALLOC;
			trace->ops[op_index].index = index;
			trace->ops[op_index].size = size;
			max_index = (index > max_index) ? index : max_index;
			break;
		case 'r':
			fscanf(tracefile, "%u %u", &index, &size);
			trace->ops[op_index].type = REALLOC;
			trace->ops[op_index].index = index;
			trace->ops[op_index].size = size;
			max_index = (index > max_index) ? index : max_index;
			break;
		case 'f':
			fscanf(tracefile, "%ud", &index);
			trace->ops[op_index].type = FREE;
			trace->ops[op_index].index = index;
			break;
		default:
			printf("Bogus type character (%c) in tracefile %s\n",
				   type[0], path);
			exit(1);
		}
		op_index++;
	}
	fclose(tracefile);
	assert(max_index == trace->num_ids - 1);
	assert(trace->num_ops == op_index);

	return trace;
}

/*
 * free_trace - Free the trace record and the three arrays it points
 * free_trace - 추적 레코드와 그것이 가리키는 세 개의 배열을 해제합니다.
 *              to, all of which were allocated in read_trace().
 *              이들은 모두 read_trace()에서 할당되었습니다.
 */
void free_trace(trace_t *trace)
{
	free(trace->ops); /* free the three arrays... */ /* 세 개의 배열을 해제합니다... */
	free(trace->blocks);
	free(trace->block_sizes);
	free(trace); /* and the trace record itself... */ /* 그리고 추적 레코드 자체도 해제합니다... */
}

/**********************************************************************
 * The following functions evaluate the correctness, space utilization,
 * 아래 함수들은 libc와 mm malloc 패키지의 정확성, 공간 활용도,
 * and throughput of the libc and mm malloc packages.
 * 처리량을 평가합니다.
 **********************************************************************/

/*
 * eval_mm_valid - Check the mm malloc package for correctness
 * eval_mm_valid - mm malloc 패키지의 정확성을 검사합니다
 */
static int eval_mm_valid(trace_t *trace, int tracenum, range_t **ranges)
{
	int i, j;
	int index;
	int size;
	int oldsize;
	char *newp;
	char *oldp;
	char *p;

	/* Reset the heap and free any records in the range list */
	/* 힙을 초기화하고 범위 목록의 모든 기록을 해제합니다 */
	mem_reset_brk();
	clear_ranges(ranges);

	/* Call the mm package's init function */
	/* mm 패키지의 초기화 함수를 호출합니다 */
	if (mm_init() < 0)
	{
		malloc_error(tracenum, 0, "mm_init failed.");
		return 0;
	}

	/* Interpret each operation in the trace in order */
	/* 추적의 각 연산을 순서대로 해석합니다 */
	for (i = 0; i < trace->num_ops; i++)
	{
		index = trace->ops[i].index;
		size = trace->ops[i].size;

		switch (trace->ops[i].type)
		{

		case ALLOC: /* mm_malloc */ /* mm_malloc 호출 */

			/* Call the student's malloc */
			/* 학생의 malloc을 호출합니다 */
			if ((p = mm_malloc(size)) == NULL)
			{
				malloc_error(tracenum, i, "mm_malloc failed.");
				return 0;
			}

			/*
			 * Test the range of the new block for correctness and add it
			 * 새 블록의 범위를 정확성 측면에서 검사하고, 문제가 없으면
			 * to the range list if OK. The block must be  be aligned properly,
			 * 범위 목록에 추가합니다. 블록은 올바르게 정렬되어야 하며,
			 * and must not overlap any currently allocated block.
			 * 현재 할당된 어떤 블록과도 겹치면 안 됩니다.
			 */
			if (add_range(ranges, p, size, tracenum, i) == 0)
				return 0;

			/* ADDED: cgw
			 * 추가: cgw
			 * fill range with low byte of index.  This will be used later
			 * 범위를 인덱스의 하위 1바이트 값으로 채웁니다. 이는 나중에
			 * if we realloc the block and wish to make sure that the old
			 * 블록을 realloc했을 때 이전 데이터가
			 * data was copied to the new block
			 * 새 블록으로 복사되었는지 확인하는 데 사용됩니다
			 */
			memset(p, index & 0xFF, size);

			/* Remember region */
			/* 영역 정보를 기억합니다 */
			trace->blocks[index] = p;
			trace->block_sizes[index] = size;
			break;

		case REALLOC: /* mm_realloc */ /* mm_realloc 호출 */

			/* Call the student's realloc */
			/* 학생의 realloc을 호출합니다 */
			oldp = trace->blocks[index];
			if ((newp = mm_realloc(oldp, size)) == NULL)
			{
				malloc_error(tracenum, i, "mm_realloc failed.");
				return 0;
			}

			/* Remove the old region from the range list */
			/* 범위 목록에서 이전 영역을 제거합니다 */
			remove_range(ranges, oldp);

			/* Check new block for correctness and add it to range list */
			/* 새 블록의 정확성을 검사하고 범위 목록에 추가합니다 */
			if (add_range(ranges, newp, size, tracenum, i) == 0)
				return 0;

			/* ADDED: cgw
			 * 추가: cgw
			 * Make sure that the new block contains the data from the old
			 * 새 블록에 이전 블록의 데이터가 들어 있는지 확인한 뒤,
			 * block and then fill in the new block with the low order byte
			 * 새 블록을 새 인덱스의 하위 바이트 값으로
			 * of the new index
			 * 채웁니다
			 */
			oldsize = trace->block_sizes[index];
			if (size < oldsize)
				oldsize = size;
			for (j = 0; j < oldsize; j++)
			{
				if (newp[j] != (index & 0xFF))
				{
					malloc_error(tracenum, i, "mm_realloc did not preserve the "
											  "data from old block");
					return 0;
				}
			}
			memset(newp, index & 0xFF, size);

			/* Remember region */
			/* 영역 정보를 기억합니다 */
			trace->blocks[index] = newp;
			trace->block_sizes[index] = size;
			break;

		case FREE: /* mm_free */ /* mm_free 호출 */

			/* Remove region from list and call student's free function */
			/* 목록에서 영역을 제거하고 학생의 free 함수를 호출합니다 */
			p = trace->blocks[index];
			remove_range(ranges, p);
			mm_free(p);
			break;

		default:
			app_error("Nonexistent request type in eval_mm_valid");
		}
	}

	/* As far as we know, this is a valid malloc package */
	/* 현재까지 확인한 바로는 유효한 malloc 패키지입니다 */
	return 1;
}

/*
 * eval_mm_util - Evaluate the space utilization of the student's package
 * eval_mm_util - 학생 패키지의 공간 활용도를 평가합니다
 *   The idea is to remember the high water mark "hwm" of the heap for
 *   핵심 아이디어는 최적의 할당기, 즉 빈 공간과 내부 단편화가 없는 경우의
 *   an optimal allocator, i.e., no gaps and no internal fragmentation.
 *   힙의 최고 사용량 "hwm"을 기억하는 것입니다.
 *   Utilization is the ratio hwm/heapsize, where heapsize is the
 *   활용도는 hwm/heapsize 비율이며, 여기서 heapsize는
 *   size of the heap in bytes after running the student's malloc
 *   학생의 malloc 패키지를 추적에 대해 실행한 뒤의
 *   package on the trace. Note that our implementation of mem_sbrk()
 *   힙 바이트 크기입니다. 또한 mem_sbrk() 구현은
 *   doesn't allow the students to decrement the brk pointer, so brk
 *   학생이 brk 포인터를 감소시키는 것을 허용하지 않으므로,
 *   is always the high water mark of the heap.
 *   brk는 항상 힙의 최고 사용량이 됩니다.
 *
 */
static double eval_mm_util(trace_t *trace, int tracenum, range_t **ranges)
{
	int i;
	int index;
	int size, newsize, oldsize;
	int max_total_size = 0;
	int total_size = 0;
	char *p;
	char *newp, *oldp;

	/* initialize the heap and the mm malloc package */
	/* 힙과 mm malloc 패키지를 초기화합니다 */
	mem_reset_brk();
	if (mm_init() < 0)
		app_error("mm_init failed in eval_mm_util");

	for (i = 0; i < trace->num_ops; i++)
	{
		switch (trace->ops[i].type)
		{

		case ALLOC: /* mm_alloc */ /* mm_alloc 호출 */
			index = trace->ops[i].index;
			size = trace->ops[i].size;

			if ((p = mm_malloc(size)) == NULL)
				app_error("mm_malloc failed in eval_mm_util");

			/* Remember region and size */
			/* 영역과 크기를 기억합니다 */
			trace->blocks[index] = p;
			trace->block_sizes[index] = size;

			/* Keep track of current total size
			 * 현재 총 크기, 즉
			 * of all allocated blocks */
			/* 모든 할당 블록 크기의 합을 추적합니다 */
			total_size += size;

			/* Update statistics */
			/* 통계를 갱신합니다 */
			max_total_size = (total_size > max_total_size) ? total_size : max_total_size;
			break;

		case REALLOC: /* mm_realloc */ /* mm_realloc 호출 */
			index = trace->ops[i].index;
			newsize = trace->ops[i].size;
			oldsize = trace->block_sizes[index];

			oldp = trace->blocks[index];
			if ((newp = mm_realloc(oldp, newsize)) == NULL)
				app_error("mm_realloc failed in eval_mm_util");

			/* Remember region and size */
			/* 영역과 크기를 기억합니다 */
			trace->blocks[index] = newp;
			trace->block_sizes[index] = newsize;

			/* Keep track of current total size
			 * 현재 총 크기, 즉
			 * of all allocated blocks */
			/* 모든 할당 블록 크기의 합을 추적합니다 */
			total_size += (newsize - oldsize);

			/* Update statistics */
			/* 통계를 갱신합니다 */
			max_total_size = (total_size > max_total_size) ? total_size : max_total_size;
			break;

		case FREE: /* mm_free */ /* mm_free 호출 */
			index = trace->ops[i].index;
			size = trace->block_sizes[index];
			p = trace->blocks[index];

			mm_free(p);

			/* Keep track of current total size
			 * 현재 총 크기, 즉
			 * of all allocated blocks */
			/* 모든 할당 블록 크기의 합을 추적합니다 */
			total_size -= size;

			break;

		default:
			app_error("Nonexistent request type in eval_mm_util");
		}
	}

	return ((double)max_total_size / (double)mem_heapsize());
}

/*
 * eval_mm_speed - This is the function that is used by fcyc()
 * eval_mm_speed - fcyc()가 사용하는 함수로,
 *    to measure the running time of the mm malloc package.
 *    mm malloc 패키지의 실행 시간을 측정합니다.
 */
static void eval_mm_speed(void *ptr)
{
	int i, index, size, newsize;
	char *p, *newp, *oldp, *block;
	trace_t *trace = ((speed_t *)ptr)->trace;

	/* Reset the heap and initialize the mm package */
	/* 힙을 초기화하고 mm 패키지를 초기화합니다 */
	mem_reset_brk();
	if (mm_init() < 0)
		app_error("mm_init failed in eval_mm_speed");

	/* Interpret each trace request */
	/* 각 추적 요청을 해석합니다 */
	for (i = 0; i < trace->num_ops; i++)
		switch (trace->ops[i].type)
		{

		case ALLOC: /* mm_malloc */ /* mm_malloc 호출 */
			index = trace->ops[i].index;
			size = trace->ops[i].size;
			if ((p = mm_malloc(size)) == NULL)
				app_error("mm_malloc error in eval_mm_speed");
			trace->blocks[index] = p;
			break;

		case REALLOC: /* mm_realloc */ /* mm_realloc 호출 */
			index = trace->ops[i].index;
			newsize = trace->ops[i].size;
			oldp = trace->blocks[index];
			if ((newp = mm_realloc(oldp, newsize)) == NULL)
				app_error("mm_realloc error in eval_mm_speed");
			trace->blocks[index] = newp;
			break;

		case FREE: /* mm_free */ /* mm_free 호출 */
			index = trace->ops[i].index;
			block = trace->blocks[index];
			mm_free(block);
			break;

		default:
			app_error("Nonexistent request type in eval_mm_valid");
		}
}

/*
 * eval_libc_valid - We run this function to make sure that the
 * eval_libc_valid - 이 함수는
 *    libc malloc can run to completion on the set of traces.
 *    libc malloc이 추적 집합 전체를 끝까지 수행할 수 있는지 확인하기 위해 실행합니다.
 *    We'll be conservative and terminate if any libc malloc call fails.
 *    보수적으로 접근하여 libc malloc 호출이 하나라도 실패하면 종료합니다.
 *
 */
static int eval_libc_valid(trace_t *trace, int tracenum)
{
	int i, newsize;
	char *p, *newp, *oldp;

	for (i = 0; i < trace->num_ops; i++)
	{
		switch (trace->ops[i].type)
		{

		case ALLOC: /* malloc */ /* malloc 호출 */
			if ((p = malloc(trace->ops[i].size)) == NULL)
			{
				malloc_error(tracenum, i, "libc malloc failed");
				unix_error("System message");
			}
			trace->blocks[trace->ops[i].index] = p;
			break;

		case REALLOC: /* realloc */ /* realloc 호출 */
			newsize = trace->ops[i].size;
			oldp = trace->blocks[trace->ops[i].index];
			if ((newp = realloc(oldp, newsize)) == NULL)
			{
				malloc_error(tracenum, i, "libc realloc failed");
				unix_error("System message");
			}
			trace->blocks[trace->ops[i].index] = newp;
			break;

		case FREE: /* free */ /* free 호출 */
			free(trace->blocks[trace->ops[i].index]);
			break;

		default:
			app_error("invalid operation type  in eval_libc_valid");
		}
	}

	return 1;
}

/*
 * eval_libc_speed - This is the function that is used by fcyc() to
 * eval_libc_speed - fcyc()가 사용하는 함수로,
 *    measure the running time of the libc malloc package on the set
 *    추적 집합에서 libc malloc 패키지의 실행 시간을 측정합니다.
 *    of traces.
 */
static void eval_libc_speed(void *ptr)
{
	int i;
	int index, size, newsize;
	char *p, *newp, *oldp, *block;
	trace_t *trace = ((speed_t *)ptr)->trace;

	for (i = 0; i < trace->num_ops; i++)
	{
		switch (trace->ops[i].type)
		{
		case ALLOC: /* malloc */ /* malloc 호출 */
			index = trace->ops[i].index;
			size = trace->ops[i].size;
			if ((p = malloc(size)) == NULL)
				unix_error("malloc failed in eval_libc_speed");
			trace->blocks[index] = p;
			break;

		case REALLOC: /* realloc */ /* realloc 호출 */
			index = trace->ops[i].index;
			newsize = trace->ops[i].size;
			oldp = trace->blocks[index];
			if ((newp = realloc(oldp, newsize)) == NULL)
				unix_error("realloc failed in eval_libc_speed\n");

			trace->blocks[index] = newp;
			break;

		case FREE: /* free */ /* free 호출 */
			index = trace->ops[i].index;
			block = trace->blocks[index];
			free(block);
			break;
		}
	}
}

/*************************************
 * Some miscellaneous helper routines
 * 여러 가지 보조 루틴
 ************************************/

/*
 * printresults - prints a performance summary for some malloc package
 * printresults - 어떤 malloc 패키지의 성능 요약을 출력합니다
 */
static void printresults(int n, stats_t *stats)
{
	int i;
	double secs = 0;
	double ops = 0;
	double util = 0;

	/* Print the individual results for each trace */
	/* 각 추적에 대한 개별 결과를 출력합니다 */
	printf("%5s%7s %5s%8s%10s%6s\n",
		   "trace", " valid", "util", "ops", "secs", "Kops");
	for (i = 0; i < n; i++)
	{
		if (stats[i].valid)
		{
			printf("%2d%10s%5.0f%%%8.0f%10.6f%6.0f\n",
				   i,
				   "yes",
				   stats[i].util * 100.0,
				   stats[i].ops,
				   stats[i].secs,
				   (stats[i].ops / 1e3) / stats[i].secs);
			secs += stats[i].secs;
			ops += stats[i].ops;
			util += stats[i].util;
		}
		else
		{
			printf("%2d%10s%6s%8s%10s%6s\n",
				   i,
				   "no",
				   "-",
				   "-",
				   "-",
				   "-");
		}
	}

	/* Print the aggregate results for the set of traces */
	/* 추적 집합 전체의 종합 결과를 출력합니다 */
	if (errors == 0)
	{
		printf("%12s%5.0f%%%8.0f%10.6f%6.0f\n",
			   "Total       ",
			   (util / n) * 100.0,
			   ops,
			   secs,
			   (ops / 1e3) / secs);
	}
	else
	{
		printf("%12s%6s%8s%10s%6s\n",
			   "Total       ",
			   "-",
			   "-",
			   "-",
			   "-");
	}
}

/*
 * app_error - Report an arbitrary application error
 * app_error - 임의의 애플리케이션 오류를 보고합니다
 */
void app_error(char *msg)
{
	printf("%s\n", msg);
	exit(1);
}

/*
 * unix_error - Report a Unix-style error
 * unix_error - Unix 스타일 오류를 보고합니다
 */
void unix_error(char *msg)
{
	printf("%s: %s\n", msg, strerror(errno));
	exit(1);
}

/*
 * malloc_error - Report an error returned by the mm_malloc package
 * malloc_error - mm_malloc 패키지가 반환한 오류를 보고합니다
 */
void malloc_error(int tracenum, int opnum, char *msg)
{
	errors++;
	printf("ERROR [trace %d, line %d]: %s\n", tracenum, LINENUM(opnum), msg);
}

/*
 * usage - Explain the command line arguments
 * usage - 명령줄 인수를 설명합니다
 */
static void usage(void)
{
	fprintf(stderr, "Usage: mdriver [-hvVal] [-f <file>] [-t <dir>]\n");
	fprintf(stderr, "Options\n");
	fprintf(stderr, "\t-a         Don't check the team structure.\n");
	fprintf(stderr, "\t-f <file>  Use <file> as the trace file.\n");
	fprintf(stderr, "\t-g         Generate summary info for autograder.\n");
	fprintf(stderr, "\t-h         Print this message.\n");
	fprintf(stderr, "\t-l         Run libc malloc as well.\n");
	fprintf(stderr, "\t-t <dir>   Directory to find default traces.\n");
	fprintf(stderr, "\t-v         Print per-trace performance breakdowns.\n");
	fprintf(stderr, "\t-V         Print additional debug info.\n");
}
