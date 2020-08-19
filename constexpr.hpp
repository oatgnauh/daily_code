const int department = 1;
const int module = 21;
#define ERROR_CODE(code) Getcode(code)
constexpr int Getcode(const int code)
{
	return (department << 24 & 0xFF000000) | (module << 16 & 0x00FF0000) | (code & 0x0000FFFF);
}

extern "C"
{
#define ErroNetWork 101
#define ErroDisk    102

	int fun()
	{
		//something
		return ERROR_CODE(ErroNetWork);
	}
}