#include <iostream>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <string>

template <typename Message>
class Subscriber
{
public:
       virtual void onChange (Message& m) = 0;
       virtual ~Subscriber ();
};

template <typename Message>
class Observer
{
public:
       void subscribe(Subscriber<Message>* sub);
       void publish (Message& m);
private:
        typedef std::list <Subscriber<Message> *>  SubscriberList;
        SubscriberList subList_;
};


struct BookMsg
{
	BookMsg (std::string time, char side)
	  : timestamp_(time), side_(side) {}
	std::string timestamp_;
	char side_;
};

struct AddOrder
{
	std::string timestamp_;
	std::string orderId_;
    char type_;
    double price_;
    uint64_t size_;
};

struct ReduceOrder
{
    std::string timestamp_;
    std::string orderId_;
    uint32_t size_;
};

class OrderBook
{
public:

   OrderBook(Observer<BookMsg> *ob) :
      publisher_(ob), 
      bid_(*(new Side(*this, 'B'))),
      ask_(*(new Side(*this, 'S')))
   {
   }
   void addOrder(AddOrder& addOrder);
   void reduceOrder(ReduceOrder& reduceOrder);
   double getTotal (uint32_t targetSize,
                                 char side);
private:
    class Side
    {
    public:
      Side(OrderBook& book, char type);
      void addOrder(AddOrder& order);
      void reduceOrder(double price, uint64_t size);
      double getTotal (uint32_t size);
      
      class PriceLevel;
      typedef std::list <PriceLevel *> PriceLevelList;

      struct PriceLevel
      {
           PriceLevel(double& price,  uint64_t volume) : price_(price), totalVolume_(volume), numOrders_(1) {}
           void modLevel (uint64_t& volume) { totalVolume_ += volume; }
           void reduceLevel (uint64_t& volume) { totalVolume_ -= volume; }
           uint64_t getVolume() { return totalVolume_; }
           double price_;
           uint32_t numOrders_;
           uint64_t totalVolume_;
           PriceLevelList::iterator levelListIter_;
      }; // End of PriceLevel

    private:
        OrderBook& orderBook_;
        typedef std::unordered_map<double, PriceLevel *> PriceLevelMap;
        PriceLevelMap priceLevelMap_;
        PriceLevelList priceLevelList_;
        typedef bool (*SorterFunc)(PriceLevel*, PriceLevel*);
        SorterFunc sorterFunc_;
        static bool ascendingLevel(PriceLevel *level1, PriceLevel *level2);
        static bool descendingLevel(PriceLevel *level1, PriceLevel *level2);
        uint64_t totalVolume_;
    };  // End of OrderBook::Side

    
    struct Order
    {
      	Order (AddOrder& order)
      	{
            orderId_ = order.orderId_;
            type_ = order.type_;
            price_ = order.price_;
            size_ = order.size_;
      	}
      	std::string orderId_;
      	double price_;
      	uint64_t  size_;
      	char type_;
    };

    typedef std::unordered_map<std::string, Order*> OrderBookMap;
    OrderBookMap orderBookMap_;
    Side& bid_;
    Side& ask_;
    Observer <BookMsg> *publisher_;
};

class InputProcessor
{
public:
    static const uint16_t MSG_TYPE_POS;
    InputProcessor (OrderBook& order) :
              orderBook_(order) {}
    void process();
    void processAddOrder (std::string& input);
    void processReduceOrder (std::string& input);
private:
    OrderBook& orderBook_;
};

class Pricer : public  Subscriber <BookMsg>
{
public:
    Pricer (Observer <BookMsg>* ob, OrderBook& order, uint32_t targetSize)
        : orderBook_(order),  
        targetSize_(targetSize),
        sellingIncome_(0),
        buyingExpense_(0),
        observer_(ob)
        {
            observer_->subscribe(this);
        }
        
    ~Pricer();
    void calculateIncomeExpense ();
    void onChange (BookMsg& msg);
private:
    OrderBook& orderBook_;
    uint32_t targetSize_;
    double sellingIncome_;
    double buyingExpense_;
    Observer<BookMsg>* observer_;
};

class StringTokenizer
{
public:
	StringTokenizer(const char * delim) : delim_(delim) {}
	void tokenize (std::string& str,
	           std::vector <std::string>& cont);
private:
     const char *delim_;
};
