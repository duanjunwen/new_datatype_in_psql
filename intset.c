#include "postgres.h"
#include "fmgr.h"
#include <stdio.h>
#include <stdlib.h>
#include "libpq/pqformat.h"		/* needed for send/recv functions */
#include "access/hash.h"
#include "utils/builtins.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

PG_MODULE_MAGIC;

typedef struct intSet
{
	int		length;      // header
	int     set_length;  // number of element in array
	int     long_or_short; // beyond 10000 is long:1, below is short:0
	int		intset[10];  //assume 10 element
}intSet;

//Bubblesort reference from https://blog.csdn.net/37964044/article

// the approach to malloc array reference from  https://zhuanlan.zhihu.com/p/134239145
//if (length_array == length)
//    length = length*2;
//    array = (int *)realloc(array, sizeof(int)*length);


void Bubblesort(int *arr, int sz);
bool valid_bracket(char *str, int len);
bool check_is_all_digit(char *str, int len);
bool check_is_all_positive_int(char *str);
bool check_comma(char *str);

void Bubblesort(int *arr, int sz)
{
    for (int j = sz - 1; j >= 0; j--){
        for (int i = 0; i+1 <= j; i++){
            if (arr[i] > arr[i + 1]){
                int tmp = arr[i];
                arr[i] = arr[i + 1];
                arr[i + 1] = tmp;
            }
        }
    }
}

//check left bracket = right bracket = 1
bool valid_bracket(char *str, int len){
    int left_bracket = 0;
    int right_bracket = 0;

    for (int i = 0; i < len; i ++){
        if (str[i] == '{'){
            left_bracket ++;
        }
        if (str[i] == '}'){
            right_bracket ++;
        }
    }
    if ((left_bracket == right_bracket) &&(left_bracket == 1)&&(right_bracket == 1)){
        return true;
    }
    return false;
}

//check every char is digit, could be int ，float， negative, can not be a-z
bool check_is_all_digit(char *str, int len){
    bool is_all_digit = true;
    for (int i = 0; i < len; i ++){
        //is current char is not in '{ ,}',it must be digit,"." (for float or double), or "-" (for negative)
        if ( (str[i] != '{') && (str[i] != '}') && (str[i] != ' ') && (str[i] != ',')  ){
            if (isdigit(str[i])){  // is digit
                continue;
            }else if ((str[i] == '.') || (str[i] == '-')){
                //cannot be float,double and negative
                is_all_digit = false;
                break;
            }else{
                // not digit ,"." and "-"
                is_all_digit = false;
                break;
            }
        }
    }
    return is_all_digit;
}

//check is all positive
bool check_is_all_positive_int(char *str){
    char *delim = "{, }";  //split delim
    char *token;  //get every element, "1", "2", "3"
    double element;  //atof change token to int
    bool curr_element_is_digit;
    int len_token;
    bool all_element_is_positive_int;

    token = strtok(str, delim);

    while( token != NULL ) {
        //printf("curr_token is :%s\n", token);
        len_token = strlen(token);
        //check current token is correct digit
        curr_element_is_digit = check_is_all_digit(token, len_token);
        //if correct digit，it could be float or negative(acuracly will not ,check previous ,but just in case)
        if (curr_element_is_digit){
            element = atof(token);  //change to float
            //if negative
            if (element < 0){
                all_element_is_positive_int = false;
                //printf("curr_element is negative");
                break;
            }else{  //if positive
                //positive int
                if ((int) element == element){
                    all_element_is_positive_int = true;

                    //printf("curr_element is positive int");
                    //printf("%f\n", element);
                }else{  //positive float
                    //printf("curr_element is positive float");
                    all_element_is_positive_int = false;
                    break;
                }
            }
        }else{
            //not a digit
            all_element_is_positive_int = false;
            break;
        }
        token = strtok(NULL, delim);
    }
    return all_element_is_positive_int;
}

// comma_count= digit number-1 (element_count set)
// or comma_count = element_count = 0 (empty set)
bool check_comma(char *str){
    int length = strlen(str);
    int comma_count = 0; //number of ','
    int element_count= 0; //number of digit
    const char s[5] = "{, }";
    char *token;


    for (int i = 0; i < length; i++) {
        if (str[i] == ','){
            comma_count ++;
        }
    }

    token = strtok(str, s);

    while( token != NULL ) {
        //printf( "%s\n", token );
        element_count ++;
        token = strtok(NULL, s);
    }

    if ( (comma_count == (element_count - 1)) || ((comma_count == 0) && (element_count == 0)) ){
        return true;
    }
    else{
        return false;
    }
}

/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(intset_in);

Datum
intset_in(PG_FUNCTION_ARGS)
{
	char	   *str = PG_GETARG_CSTRING(0); // 输入字符串 "{1，2，3}"
	intSet   *result;  //输出结果intSet结构


	char *delim = "{, }";  //split delim
	char *token;  //每个当前取下的字符, "1", "2", "3"

	int *array;  //存整数数组[1,2,3]
	int length;  //假定整数数组array长度为2

	int length_array = 0;  //起始 元素个数 = 0
	int element = 0;  //当前遍历出来的字符转化成int
    int already_exist; //检查整数是否已经存在
    int curr_sub_length = 0;

    int size_of_intset_header;
    int size_of_intset_set_length;
    int size_of_int_set;
    int long_or_short;
    int sum_size;

    //check 字符串是否有错
    // assume all true
    bool correct_bracket = true;
    bool is_all_digit = true;
    bool is_all_positive_int = true;
    bool correct_comma = true;
    int str_len = strlen(str);

    char *str_copy_for_pos_int;
    char *str_copy_for_check_comma;

    str_copy_for_pos_int = malloc(sizeof(char) * str_len * 10);  //此处malloc
    str_copy_for_check_comma = malloc(sizeof(char) * str_len * 10);  //此处malloc
    strcpy(str_copy_for_pos_int, str);
    strcpy(str_copy_for_check_comma, str);

    //correct bracket number is 2
    correct_bracket = valid_bracket(str, str_len);
    //all element is positive int, wrong case:3.14, -3.14,
    is_all_digit = check_is_all_digit(str, str_len);
    if (is_all_digit){
        is_all_positive_int = check_is_all_positive_int(str_copy_for_pos_int);
    } else{
        is_all_positive_int = false;
    }
    // is_all_positive_int = check_is_all_positive_int(str_copy_for_pos_int);
    correct_comma = check_comma(str_copy_for_check_comma);

    free(str_copy_for_pos_int);  //此处free str_copy_for_pos_int
    free(str_copy_for_check_comma);  //此处free str_copy_for_check_comma

	if ((correct_bracket == false) || (is_all_positive_int == false) || (correct_comma == false))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type %s: \"%s\"",
						"intSet", str)));


    length = 10;
    length_array = 0;
    already_exist = 0;
	array = malloc(sizeof(int) * length);  // 先分配length长度的数组
    //array = palloc(sizeof(int) * length);  // 先分配length长度的数组

    token = strtok(str, delim);
    /* 继续获取其他的子字符串 */
    while( token != NULL ) {
        element = atoi(token);  //当前token，转化为int

        //如果array大小不够,realloc
        if (length_array == length){
            length = length + 10;
            array = (int *)realloc(array, sizeof(int)*length);
            //repalloc
            //array = repalloc(array, sizeof(int)*length);
        }
        //假定不重复
        already_exist = 0;
        //去重操作，查看字符串是否已经存在
        //遍历当前数组
        curr_sub_length = length_array;
        for (int i = 0; i < curr_sub_length; i++) {

            if (curr_sub_length == length){
                length = length + 10;
                array = realloc(array, sizeof(int)*length);
            }

            if (array[i] == element){
                already_exist = 1;
                break;
            } else{
                already_exist = 0;
            }

        }
        //当前element不在array里，存入array
        if (already_exist == 0){
            //把token转化为整数，存入int数组

            if (curr_sub_length == length){
                length = length + 10;
                array = realloc(array, sizeof(int)*length);
            }else if (curr_sub_length == length){
                length = length + 10;
                array = realloc(array, sizeof(int)*length);
            }

            array[curr_sub_length] = element;  //save element in array
            length_array++;
            curr_sub_length++;
        } else{
            element = 0;
        }

        if (length_array == length){
            length = length + 10;
            array = realloc(array, sizeof(int)*length);
        }else if (curr_sub_length == length){
            length = length + 10;
            array = realloc(array, sizeof(int)*length);
        }

        token = strtok(NULL, delim);
    }

    //把array存到intSet结构
    //如果array长度位5，
    //设置intSet长度为6，第0个元素为数组的长度
    //1-5为数组内容
    size_of_intset_header = VARHDRSZ;
    size_of_intset_set_length = sizeof(int);
    size_of_int_set = sizeof(int) * (length_array + 1);
    long_or_short = sizeof(int);
    sum_size = size_of_intset_header + size_of_intset_set_length + size_of_int_set + long_or_short;

    result = (intSet *)palloc(sum_size);
    SET_VARSIZE(result, sum_size);
    //result = (intSet *)palloc(VARHDRSZ + sizeof(int) +(sizeof(int) * (length_array + 1)));
    //SET_VARSIZE(result, VARHDRSZ + sizeof(int) + (sizeof(int) * (length_array + 1)));
    //intSet *result第0位为array长度
    Bubblesort(array, length_array); // sort the array
    result->intset[0] = length_array;
    result->set_length = length_array;
    if (length_array >= 10000){
        result->long_or_short = 1;
    } else{
        result->long_or_short = 0;
    }

    for (int i = 0; i < length_array; i++){
        //第0位为array长度所以从第1位分配
        result->intset[i + 1] = array[i];
    }
    free(array);
	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(intset_out);

Datum
intset_out(PG_FUNCTION_ARGS)
{
	intSet    *curr_intSet = (intSet *) PG_GETARG_POINTER(0);
    char      first_str_num[32];
    char      curr_str_num[32];  //wont be longer than 32
    char	  *result;
    int       array_length = curr_intSet->set_length;

    result = palloc(sizeof(curr_str_num) * array_length * 2);
	result[0] = '\0';  //str tail should be '\0'

    if (array_length > 0){
        // get first element
        pg_ltoa(curr_intSet->intset[1], first_str_num);
    } else{
        // array = 0, empty set
        strcat(result, "{}");
        PG_RETURN_CSTRING(psprintf("%s", result));
    }

	if (array_length > 0) {
        if (array_length == 1){
            // one element set not need comma
            strcat(result, "{");
            strcat(result, first_str_num);
            strcat(result, "}");
        }
        else if (array_length > 1) {
            // need comma
            strcat(result, "{");
            // append first element
            strcat(result, first_str_num);
            //append "," + "str_number"
            for (int i = 1; i < array_length; i++) {
                strcat(result, ",");
                pg_ltoa(curr_intSet->intset[i + 1], curr_str_num);
                strcat(result, curr_str_num);
            }
            strcat(result, "}");
        }
    }
    PG_RETURN_CSTRING(psprintf("%s", result));
}

PG_FUNCTION_INFO_V1(intset_eq);

Datum
intset_eq(PG_FUNCTION_ARGS)
{
	intSet    *a = (intSet *) PG_GETARG_POINTER(0);
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);

    int length_a = a->set_length;
    int length_b = b->set_length;

    if (length_a != length_b){  //different length, not eq
        PG_RETURN_BOOL(0);
    } else{
        // same length
        //a,b are sorted
        //a eq b
        for (int i = 0; i < length_a; i++) {
            if (a->intset[i + 1] != b->intset[i + 1]){
                PG_RETURN_BOOL(0);
            }
        }

    }
	PG_RETURN_BOOL(1);
}

// # setA
//intset_cardinality返回整数集整数个数
PG_FUNCTION_INFO_V1(intset_cardinality);

Datum
intset_cardinality(PG_FUNCTION_ARGS)
{
    intSet *a = (intSet *) PG_GETARG_POINTER(0);
    //intSet的长度存在第0个元素
    int result = a->set_length;
    PG_RETURN_INT32(result);
}

//intset_difference求A和B的差集
PG_FUNCTION_INFO_V1(intset_difference);

Datum
intset_difference(PG_FUNCTION_ARGS)
{
    intSet    *a = (intSet *) PG_GETARG_POINTER(0);
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);
    intSet    *result;
    int length_a = a->set_length;
    int length_b = b->set_length;

    //
    int *array;  //数组存放在A不在B里的[1,2,3]
    int length;  //假定整数数组array长度为2

    int length_array = 0;  //记录array元素个数
    int both_exist = 0; //检查整数是否已经存在

    int size_of_intset_header;
    int size_of_intset_set_length;
    int size_of_int_set;
    int long_or_short;
    int sum_size;


    if (length_a > length_b){
        length = length_b;
    } else{
        length = length_a;
    }

    array = malloc(sizeof(int) * length);  // 先分配length长度的数组
    //遍历a
    for (int i = 0; i < length_a; i++) {
        if (length_array == length){
            length = length + 10;
            array = realloc(array, sizeof(int)*length);
        }
        //假设当前元素a[i + 1]不在b中
        both_exist = 0;
        //遍历b
        for (int j = 0; j < length_b; j++) {
            //如果
            if (a->intset[i + 1] == b->intset[j + 1]){
                both_exist = 1;
            }
        }

        //遍历完b
        //如果both_have = 0，则a[i + 1]不在b[i + 1],是差集，加入array
        if (both_exist == 0){
            //判断array长度是否还够
            //element_counts是array当前int个数
            if (length_array == length){
                length = length + 10;
                array = realloc(array, sizeof(int)*length);
            }
            array[length_array] = a->intset[i + 1];
            length_array ++;
        }
    }

    size_of_intset_header = VARHDRSZ;
    size_of_intset_set_length = sizeof(int);
    size_of_int_set = sizeof(int) * (length_array + 1);
    long_or_short = sizeof(int);
    sum_size = size_of_intset_header + size_of_intset_set_length + size_of_int_set + long_or_short;

    result = (intSet *)palloc(sum_size);
    SET_VARSIZE(result, sum_size);

    //intSet *result第0位为array长度
    result->intset[0] = length_array;
    result->set_length = length_array;
    if (length_array >= 10000){
        result->long_or_short = 1;
    } else{
        result->long_or_short = 0;
    }
    for (int i = 0; i < length_array; i++){
        //第0位为array长度所以从第1位分配
        result->intset[i + 1] = array[i];
    }
    free(array);
    PG_RETURN_POINTER(result);
}

//i ? intSet
PG_FUNCTION_INFO_V1(intset_i_in_set);

Datum
intset_i_in_set(PG_FUNCTION_ARGS)
{
    int    a = PG_GETARG_INT32(0);  //int a
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);
    int result = 0;  //false,int a not in set b
    int length_b = b->set_length;

    if (length_b > 0) {
        if (length_b == 0){
            result = 0;
        } else {
            for (int i = 0; i < length_b; i++) {
                if (a == b->intset[i + 1]) {
                    // a in set b
                    result = 1;
                    break;
                }
            }
        }
    } else{
        result = 0;
    }
    PG_RETURN_BOOL(result);
}

//A >@ B, A contain B
PG_FUNCTION_INFO_V1(intset_a_contain_b);

Datum
intset_a_contain_b(PG_FUNCTION_ARGS)
{
    intSet    *a = (intSet *) PG_GETARG_POINTER(0);
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);
    int result = 1;  //默认A contain B
    int curr_contain; //默认当前元素B有,A没有

    int length_a = a->set_length;
    int length_b = b->set_length;

    if (length_a < length_b){  //A比B小，A一定不containB
        result = 0;
    } else{
        //a,b是排序过后的b
        //遍历B
        for (int i = 0; i < length_b; i++) {
            curr_contain = 0;//false默认当前元素B有,A没有
            //遍历A
            for (int j = 0; j < length_a; j++) {
                //如果当前element B有，A也有
                if (b->intset[i + 1]  == a->intset[j + 1]){
                    curr_contain = 1;
                }
            }
            //遍历A结束
            if (curr_contain == 0){  //如果当前element B有A没有
                result = 0;//false
                break;
            } else{
                continue;
            }
        }
    }
    PG_RETURN_BOOL(result);
}


//A @< B, A contained by B
PG_FUNCTION_INFO_V1(intset_a_be_contained_by_b);

Datum
intset_a_be_contained_by_b(PG_FUNCTION_ARGS)
{
    intSet    *a = (intSet *) PG_GETARG_POINTER(0);
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);

    int length_a = a->set_length;
    int length_b = b->set_length;

    int result = 1;  //默认A be contained by B
    int curr_contain; //默认当前元素A有,B没有

    if (length_a > length_b){  //A比B大，A一定不be contain byB
        result = 0;
    } else{
        //a,b是排序过后的b
        //遍历A
        for (int i = 0; i < length_a; i++) {
            curr_contain = 0;//false默认当前元素A有,B没有
            //遍历B
            for (int j = 0; j < length_b; j++) {
                //如果当前element B有，A也有
                if (a->intset[i + 1]  == b->intset[j + 1]){
                    curr_contain = 1;
                }
            }
            //遍历A结束
            if (curr_contain == 0){  //如果当前element A有B没有
                result = 0;//false
                break;
            } else{
                continue;
            }
        }
    }
    PG_RETURN_BOOL(result);
}

//A <> B
//1 是true A,B不相等
//0 是false A,B 相等
PG_FUNCTION_INFO_V1(intset_not_eq);

Datum
intset_not_eq(PG_FUNCTION_ARGS)
{
    intSet    *a = (intSet *) PG_GETARG_POINTER(0);
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);
    int result = 0;  //默认false，即a=b相等

    int length_a = a->set_length;
    int length_b = b->set_length;

    if (length_a == length_b){  //长度相等，a,b不一定不相等 {1,2,3}， {1,2,4}
        //a,b是排序过后的
        //对比每个元素
        for (int i = 0; i < length_a; i++) {
            if (a->intset[i + 1] != b->intset[i + 1]){
                result = 1;
                break;
            }
        }
    } else{  //长度不相等,a, b 不等，true
        result = 1;
    }
    PG_RETURN_BOOL(result);
}


//intset_intersection求A,B交集
PG_FUNCTION_INFO_V1(intset_intersection);

Datum
intset_intersection(PG_FUNCTION_ARGS)
{
    intSet    *a = (intSet *) PG_GETARG_POINTER(0);
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);
    intSet    *result;

    //
    int *array;  //数组存放在A不在B里的[1,2,3]
    int length;  //假定整数数组长度

    int length_array = 0;  //记录array元素个数
    int both_exist = 0; //检查整数是否已经存在

    int length_a = a->set_length;
    int length_b = b->set_length;

    int size_of_intset_header;
    int size_of_intset_set_length;
    int size_of_int_set;
    int long_or_short;
    int sum_size;


    if (length_a > length_b){
        length = length_b;
    } else{
        length = length_a;
    }

    array = malloc(sizeof(int) * length);  // 先分配length长度的数组
    //遍历a
    for (int i = 0; i < length_a; i++) {
        if (length_array == length){
            length = length + 10;
            array = realloc(array, sizeof(int)*length);
        }
        //假设当前元素a[i + 1]不在b中
        both_exist = 0;
        //遍历b
        for (int j = 0; j < length_b; j++) {
            //如果
            if (a->intset[i + 1] == b->intset[j + 1]){
                both_exist = 1;
            }
        }

        //遍历完b
        //如果both_have = 1，则在a[i + 1]也在b[i + 1],是交集元素，加入array
        if (both_exist == 1){
            //判断array长度是否还够
            //element_counts是array当前int个数
            if (length_array == length){
                length = length + 10;
                array = realloc(array, sizeof(int)*length);
            }
            array[length_array] = a->intset[i + 1];
            length_array ++;
        }
    }

    Bubblesort(array, length_array);

    size_of_intset_header = VARHDRSZ;
    size_of_intset_set_length = sizeof(int);
    size_of_int_set = sizeof(int) * (length_array + 1);
    long_or_short = sizeof(int);
    sum_size = size_of_intset_header + size_of_intset_set_length + size_of_int_set + long_or_short;

    result = (intSet *)palloc(sum_size);
    SET_VARSIZE(result, sum_size);

    //intSet *result第0位为array长度
    result->intset[0] = length_array;
    result->set_length = length_array;
    if (length_array >= 10000){
        result->long_or_short = 1;
    } else{
        result->long_or_short = 0;
    }

    for (int i = 0; i < length_array; i++){
        //第0位为array长度所以从第1位分配
        result->intset[i + 1] = array[i];
    }
    free(array);
    PG_RETURN_POINTER(result);
}


//intset_intersection求A,B并集
// 在A或者在B里
PG_FUNCTION_INFO_V1(intset_union);

Datum
intset_union(PG_FUNCTION_ARGS)
{
    intSet    *a = (intSet *) PG_GETARG_POINTER(0);
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);
    intSet    *result;


    int *array;  //数组存放在A也在B里的
    int length;  //假定整数数组

    int length_array = 0;  //记录array元素个数
    int already_exist = 0; //检查整数是否已经存在

    int length_a = a->set_length;
    int length_b = b->set_length;

    int size_of_intset_header;
    int size_of_intset_set_length;
    int size_of_int_set;
    int long_or_short;
    int sum_size;

    int curr_start = 0; //array 添加完a后,此变量作为b循环的控制变量

    if (length_a > length_b){
        length = length_a;
    } else{
        length = length_b;
    }

    array = malloc(sizeof(int) * length);  // 先分配length长度的数组
    //遍历a
    for (int i = 0; i < length_a; i++) {
        if (length_array == length){
            length = length + 10;
            array = realloc(array, sizeof(int)*length);
        }
        //直接加入array
        array[length_array] = a->intset[i + 1];
        length_array ++;
    }

    //遍历数组b
    for (int k = 0; k < length_b; k++) {
        //遍历array
        already_exist = 0;  //假设当前b[k]不存在
        for (int j = curr_start; j < length_array; j++) {
            //if the same current elements
            if (b->intset[k + 1] == array[j]){
                already_exist = 1;
                curr_start = j;
            }
        }
        //如果当前b[k+1]已经存在array
        if (already_exist == 1){
            continue;
        } else{
            if (length_array == length){
                length = length + 10;
                array = realloc(array, sizeof(int)*length);
            }
            array[length_array] = b->intset[k + 1];
            length_array ++;
        }

    }
    //排序
    Bubblesort(array, length_array);

    size_of_intset_header = VARHDRSZ;
    size_of_intset_set_length = sizeof(int);
    size_of_int_set = sizeof(int) * (length_array + 1);
    long_or_short = sizeof(int);
    sum_size = size_of_intset_header + size_of_intset_set_length + size_of_int_set + long_or_short;

    result = (intSet *)palloc(sum_size);
    SET_VARSIZE(result, sum_size);

    //intSet *result第0位为array长度
    result->intset[0] = length_array;
    result->set_length = length_array;
    if (length_array >= 10000){
        result->long_or_short = 1;
    } else{
        result->long_or_short = 0;
    }

    for (int i = 0; i < length_array; i++){
        //第0位为array长度所以从第1位分配
        result->intset[i + 1] = array[i];
    }
    free(array);
    PG_RETURN_POINTER(result);
}



//intset_difference求A和B的异或
//A-B的元素 + （并） B-A的元素
PG_FUNCTION_INFO_V1(intset_disjunction);

Datum
intset_disjunction(PG_FUNCTION_ARGS)
{
    intSet    *a = (intSet *) PG_GETARG_POINTER(0);
    intSet    *b = (intSet *) PG_GETARG_POINTER(1);
    intSet    *result;

    //
    int *array;  //数组存放在A不在B里的[1,2,3]
    int length;  //假定整数数组

    int length_a = a->set_length;
    int length_b = b->set_length;

    int length_array = 0;  //记录array元素个数
    int both_exist = 0; //检查整数是否已经存在

    int size_of_intset_header;
    int size_of_intset_set_length;
    int size_of_int_set;
    int long_or_short;
    int sum_size;

    if (length_a > length_b){
        length = length_a;
    } else{
        length = length_b;
    }

    array = malloc(sizeof(int) * length);  // 先分配length长度的数组

    //A-B
    //遍历a
    for (int i = 0; i < length_a; i++) {
        if (length_array == length){
            length = length + 10;
            array = realloc(array, sizeof(int)*length);
        }
        //假设当前元素a[i + 1]不在b中
        both_exist = 0;
        //遍历b
        for (int j = 0; j < length_b; j++) {
            //如果
            if (a->intset[i + 1] == b->intset[j + 1]){
                both_exist = 1;
            }
        }

        //遍历完b
        //如果both_have = 0，则a[i + 1]不在b[i + 1],是差集，加入array
        if (both_exist == 0){
            //判断array长度是否还够
            //element_counts是array当前int个数
            if (length_array == length){
                length = length + 10;
                array = realloc(array, sizeof(int)*length);
            }
            array[length_array] = a->intset[i + 1];
            length_array ++;
        }
    }

    //B-A
    //遍历b
    for (int i = 0; i < length_b; i++) {
        if (length_array == length){
            length = length + 10;
            array = realloc(array, sizeof(int)*length);
        }
        //假设当前元素b[i + 1]不在a中
        both_exist = 0;
        //遍历a
       for (int j = 0; j < length_a; j++) {
            //如果
            if (b->intset[i + 1] == a->intset[j + 1]){
                both_exist = 1;
            }
        }

        //遍历完a
        //如果both_have = 0，则a[i + 1]不在b[i + 1],是差集，加入array
        if (both_exist == 0){
            //判断array长度是否还够
            //element_counts是array当前int个数
            if (length_array == length){
                length = length + 10;
                array = realloc(array, sizeof(int)*length);
            }
            array[length_array] = b->intset[i + 1];
            length_array ++;
        }
    }

    Bubblesort(array, length_array);

    size_of_intset_header = VARHDRSZ;
    size_of_intset_set_length = sizeof(int);
    size_of_int_set = sizeof(int) * (length_array + 1);
    long_or_short = sizeof(int);
    sum_size = size_of_intset_header + size_of_intset_set_length + size_of_int_set + long_or_short;

    result = (intSet *)palloc(sum_size);
    SET_VARSIZE(result, sum_size);

    //intSet *result第0位为array长度
    result->intset[0] = length_array;
    result->set_length = length_array;
    if (length_array >= 10000){
        result->long_or_short = 1;
    } else{
        result->long_or_short = 0;
    }

    for (int i = 0; i < length_array; i++){
        //第0位为array长度所以从第1位分配
        result->intset[i + 1] = array[i];
    }
    free(array);
    PG_RETURN_POINTER(result);
}

