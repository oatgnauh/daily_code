#include <iostream>
#include <map>
#include <string>

#define METHOD_FUNCTION_OBJECT


//方式一：重载此作为key的结构体中的 < 操作符
struct _key
{
	_key(int a, const std::string &str)
		: first(a)
		, second(str)
	{

	}

    bool operator<(const _key & right) const;

	int first;
	std::string second;
};

#ifndef METHOD_FUNCTION_OBJECT

//在结构体中多个值都需要做唯一性比较的时候，先从第一个开始比较
//因为map是树结构，节点之间必须有序
bool _key::operator<(const _key & right) const
{
    if (first < right.first)
        return true;
    
    if (second < right.second)
        return true;
    

    return false;
}


#else
//方式二：传入一个仿函数作为map示例的第三个模板参数

struct cmp
{
    bool operator()(const _key& left, const _key& right) const  //此函数必须为const函数，否则编译会有问题。为了安全起见，也应该是const函数
    {
        if(left.first < right.first)
            return true;
        if(left.second < right.second)
            return true;
        return false;
    }

};

#endif


int main()
{
    // std::map<_key, std::string> mmp;

    std::map<_key, std::string,cmp> mmp;
	_key a = _key(1, "hello");
	_key b = _key(2, "hello1");
	_key c = _key(1, "ehello");
	mmp[a] = "test";
	mmp[b] = "world1";
	mmp[c] = "world2";


	for (auto &x : mmp)
	{
		std::cout << "key:" << x.first.first << "," << x.first.second << ". value:" << x.second << std::endl;
	}
	_key key(1, "hello2");
	auto iter = mmp.find(key);
	if (iter == mmp.end())
	{
		std::cout << iter->second << std::endl;
	}
	std::getchar();
    return 0;
}
