/* Contains resource definitions for the user interface */

/* Custom window messages */
#define WM_ADDENTRY		WM_USER
#define WM_CHANGEENTRY	(WM_USER+1)
#define EM_REDO			(WM_USER+84)
#define EM_CANREDO		(WM_USER+85)
#define EN_SELCHANGE	(WM_USER+2)
#define NM_CANUNDO		(WM_USER+3)

#define NUM_STRS		46 /* Number of strings in the string table */
#define MAIN_ICON		101
#define SPLASH_SCREEN	102

#define VERT_MARK		103
#define HORZ_MARK		104
#define CENT_MARK		105
#define U_ARROW			106
#define D_ARROW			107
#define L_ARROW			108
#define R_ARROW			109
#define UL_ARROW		110
#define UR_ARROW		111
#define DL_ARROW		112
#define DR_ARROW		113
#define V_ARROWS		114
#define H_ARROWS		115
#define CA_ARROWS		116

#define MAINMENU		117
#define ACCTABLE		118
#define SHRTCMENUS		119

#define ABOUTBOX		120
#define GEN_PARAMS_DLG	121
#define TBMP_PARAMS_DLG	122
#define TSPR_PARAMS_DLG	123
#define GAME_MODE_DLG	124

#define TREE_WINDOW		1001
#define TMLN_WINDOW		1002
#define DATA_WINDOW		1003
#define STATUS_BAR		1004
#define K_SWITCHPANES	1005
#define K_SWITCHPANES_BACK	1006
#define ID_TOOLBAR		1007

#define M_FILE_SUBM		0
#define M_NEW			2001
#define M_OPEN			2002
#define M_SAVE			2003
#define M_SAVEAS		2004
#define M_PRINT			2005
#define M_PRINT_PREV	2006
#define M_PAGE_SETUP	2007
#define M_GAME_MODE		2008
#define M_EXIT			2009
#define M_EDIT_SUBM		1
#define M_UNDO			2010
#define M_REDO			2011
#define M_CUT			2012
#define M_COPY			2013
#define M_PASTE			2014
#define M_EMPTYCLIP		2015
#define M_DELETE		2016
#define M_SELECTALL		2017
#define M_FIND			2018
#define M_FINDNEXT		2019
#define M_REPLACE		2020
#define M_RSRC_SUBM		2
#define M_RSRC_NEW		2021
#define M_RSRC_EDIT		2022
#define M_RSRC_ERAW		2023
#define M_RSRC_SAVE		2024
#define M_RSRC_REVERT	2025
#define M_RSRC_IMPORT	2026
#define M_RSRC_EXPORT	2027
#define M_VIEW_SUBM		3
#define M_STATBAR		2028
#define M_TOOLBAR		2029
#define M_TREE			2030
#define M_RSRC_PARAMS	2031
#define M_SPLIT			2032
#define M_FONT			2033
#define M_HELP_SUBM		4
#define M_HELP			2034
#define M_ABOUT			2035

#define T_NEW			2036
#define T_OPEN			2037
#define T_SAVE			2038
#define T_PRINT			2039
#define T_PRINTPRE		2040
#define T_CUT			2041
#define T_COPY			2042
#define T_PASTE			2043
#define T_UNDO			2044
#define T_REDO			2045

#define D_STATIC1		3001
#define D_STATIC2		3002
#define D_ICON1			3003

#define D_TITLE			3004
#define D_RSRC_ID_LBL	3005
#define D_RSRC_ID		3006
#define D_HAS_RSRC_NAME	3007
#define D_RSRC_NAME		3008
#define D_RSRC_TYPE_LBL	3009
#define D_RSRC_TYPE		3010
#define D_RSRC_SIZE_LBL	3011
#define D_RSRC_SIZE		3012

#define D_TBMP_WIDTH_LBL	2013
#define D_TBMP_WIDTH		2014
#define D_TBMP_HEIGHT_LBL	2015
#define D_TBMP_HEIGHT		2016
#define D_TBMP_CMPR_GROUP	2017
#define D_TBMP_BBP_LBL		2018
#define D_TBMP_BPP			2019
#define D_TBMP_HASPAL		2020
#define D_TBMP_2NDCMP_LBL	2021
#define D_TBMP_2NDCMP_NONE	2022
#define D_TBMP_2NDCMP_RLE8	2023
#define D_TBMP_2NDCMP_RLEU	2024
#define D_TBMP_1STCMP_LBL	2025
#define D_TBMP_1STCMP_NONE	2026
#define D_TBMP_1STCMP_LZ	2027
#define D_TBMP_1STCMP_LZU	2028
#define D_TBMP_1STCMP_RIVEN	2029
#define D_TBMP_PALSTAT_LBL	2030
#define D_TBMP_PALSTAT		2031
#define D_TBMP_EDITPAL		2032
#define D_TBMP_LOADPAL		2033

#define D_TSPR_NUMBMP_LBL	2034
#define D_TSPR_NUMBMP		2035
#define D_TSPR_SPRNUM		2036
#define D_TSPR_PREV			2037
#define D_TSPR_NEXT			2038
#define D_TSPR_VER_LBL		2039
#define D_TSPR_VER			2040
#define D_TSPR_UKN1_LBL		2041
#define D_TSPR_UKN1			2042
#define D_TSPR_UKN2_LBL		2043
#define D_TSPR_UKN2			2044
#define D_TSPR_UKN3_LBL		2045
#define D_TSPR_UKN3			2046
#define D_TSPR_UKN4_LBL		2047
#define D_TSPR_UKN4			2048
#define D_TSPR_UKN5_LBL		2049
#define D_TSPR_UKN5			2050
#define D_TSPR_UKN6_LBL		2051
#define D_TSPR_UKN6			2052
#define D_TSPR_UKN7_LBL		2053
#define D_TSPR_UKN7			2054
#define D_TSPR_UKN8_LBL		2055
#define D_TSPR_UKN8			2056

#define D_GM_NONE			2057
#define D_GM_ORLY			2058
