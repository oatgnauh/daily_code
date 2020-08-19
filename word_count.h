	
int GetCount()
{
    std::string fuck;
	std::cin >> fuck;

	int cnt = 0;
	while (fuck != "done")
	{
		cnt++;
		std::cin >> fuck;   //cin遇到空格会返回，连续输入一行，就做了单词的拆分
	}

    return cnt;
}
