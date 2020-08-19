#include <iostream>
#include <memory>
#include <set>
template<typename T>
class sp_ptr : public std::shared_ptr<T>
{
public:
	sp_ptr(T* ptr)
		:std::shared_ptr<T>(ptr)
	{
		std::cout << "construct, this:" << this << std::endl;;
	}
	~sp_ptr() {
		std::cout << "destroy,this:" << this <<" shared_ptr object, but use count:" << this->use_count() << std::endl;
	}
};


#define BREAK_CIRCLE_REF

class SeaKing;
class GreenTea {
public:
	GreenTea(const sp_ptr<SeaKing> &boy) :boy_(boy) {}
	~GreenTea()
	{
		std::cout << "boys backup:" << boy_.use_count() << std::endl;
	}
private:

#ifdef BREAK_CIRCLE_REF
    std::weak_ptr<SeaKing> boy_;
#else
    sp_ptr<SeaKing> boy_;
#endif
	
};

class SeaKing
{
public:
	SeaKing() {
		
	}
	void PickUpArtist(const sp_ptr<GreenTea> &girl)
	{
		girl_ocean_.insert(girl);
	}

	~SeaKing()
	{
		for(auto & tea: girl_ocean_)
			std::cout << "girls refs:" << tea.use_count() << std::endl;
	}
private:
	std::set<sp_ptr<GreenTea> > girl_ocean_;
};



int main()
{
	
    sp_ptr<SeaKing>  king(new SeaKing);
    sp_ptr<GreenTea> tea(new GreenTea(king));
    king->PickUpArtist(tea);


    return 0;
}