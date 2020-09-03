#include "orderbook.h"
#include <iomanip>

const uint16_t InputProcessor::MSG_TYPE_POS = 9;

template <typename Message>
Subscriber<Message>::~Subscriber ()
{
}

template <typename Message>
void Observer<Message>::subscribe (Subscriber<Message> *sub)
{
	subList_.push_back (sub);
}

template <typename Message>
void Observer <Message>::publish(Message& m)
{
	auto end = subList_.end ();
	for (auto it = subList_.begin(); it != end; ++it)
	{
		Subscriber<Message>*sub = *it;
	    sub->onChange(m);
	}
}

void OrderBook::addOrder(AddOrder& addOrder)
{
    Side& side      = (addOrder.type_ == 'B' ? bid_ : ask_);
    side.addOrder(addOrder);
    BookMsg msg(addOrder.timestamp_,
                                addOrder.type_);
    publisher_-> publish(msg);
}

void OrderBook::reduceOrder (ReduceOrder& order)
{
	OrderBookMap::iterator found = orderBookMap_.find (order.orderId_);
	if (found != orderBookMap_.end ())
	{
		Order* orderX = found->second;
		Side& side = (orderX->type_ == 'B' ? bid_ : ask_);
		int64_t size = orderX->size_ - order.size_;
        double price = orderX->price_;
        char type = orderX->type_;
		if (size <= 0) // Order should be removed
		{
		   orderBookMap_.erase(found);
		   delete orderX;
		}
		else
		    orderX->size_ = size;
		// find the level
		side.reduceOrder (price, order.size_);
		BookMsg msg(order.timestamp_, type);
		publisher_->publish(msg);
    }
}

double OrderBook::getTotal (uint32_t targetSize,  char type)
{
	Side& side = (type == 'B' ? bid_:ask_);
	return side.getTotal (targetSize);
}

OrderBook::Side::Side(OrderBook& orderbook, char type) : orderBook_(orderbook)
{
    if (type == 'B')
    {
        sorterFunc_ = descendingLevel;
    }
    else if (type == 'S')
    {
        sorterFunc_ = ascendingLevel;
    }
}

bool OrderBook::Side::ascendingLevel(PriceLevel *level1, PriceLevel *level2)
{
    if (level1->price_ < level2->price_)
        return true;
    return false;
}

bool OrderBook::Side::descendingLevel(PriceLevel *level1, PriceLevel *level2)
{
    if (level1->price_ > level2->price_)
        return true;
    return false;
}

void OrderBook::Side::addOrder(AddOrder& addOrder)
{
    // Insert into the order map
    Order* order = new Order(addOrder);
    totalVolume_ += order-> size_;

    std::pair<OrderBookMap::iterator, bool> orderPtr = orderBook_.orderBookMap_.insert(std::pair<std::string, Order *> (order->orderId_, order));
    if (!orderPtr.second)
    {
        if (orderPtr.first->second)
        {
            // Received a duplicate order ID ("
            // Would log this normally
            std::cerr << "Error: Duplicate OrderId " 
                            << order-> orderId_
                            << " received " 
                            << std::endl;
            return;
        }
        else
        {
            orderPtr.first->second = order;
        }
    }

    // Now see if a price level exists for this price
    PriceLevelMap::iterator found = priceLevelMap_.find(order->price_);
    if (found != priceLevelMap_.end())
    {
        // Update the level with the new order
        PriceLevel* level = found->second;
        level->modLevel(order->size_);
    }
    else
    {
        // Create a new level
        PriceLevel * level = new PriceLevel(order->price_, order->size_);
        priceLevelMap_.insert(std::make_pair(order->price_, level));
        // now this needs inserting into list
        PriceLevelList::iterator iter = 
                   std::lower_bound (priceLevelList_.begin(), 
                                     priceLevelList_.end(), 
                                     level, sorterFunc_);

        level->levelListIter_ = priceLevelList_.insert (iter, level);
    }
}

void OrderBook::Side::reduceOrder (double price, uint64_t size)
{
	totalVolume_ -= size;
    PriceLevelMap::iterator it = priceLevelMap_.find(price);
    if (it != priceLevelMap_.end())
    {
       // Adjust the totalVolume 
       PriceLevel *level = it->second;
       level->totalVolume_ -= size;
       if (level->totalVolume_ <= 0)
       {
          // Erase this price level
          priceLevelMap_.erase(it);
          // Remove from the level list 
          priceLevelList_.erase(level->levelListIter_);
       }
    }
    else
    {
        std::cerr<< "Error: No Order at price level " 
                         << price << std::endl;
    }
}

double OrderBook::Side::getTotal (uint32_t targetSize)
{
	uint64_t totalSize  = 0;
	double totalAmount = 0;
	if (totalVolume_ < targetSize)
	     return totalAmount;

	auto itr = priceLevelList_.begin ();
	auto end = priceLevelList_.end ();

	while (totalSize < targetSize && itr !=  end)
	{
		PriceLevel *level = *itr;
		if ( level->totalVolume_ + totalSize < targetSize)
		{
		    totalSize += level-> totalVolume_;
		    totalAmount += level-> totalVolume_* level-> price_;
		}
		else
		{
		    totalSize = targetSize - totalSize;
		    totalAmount +=  level-> price_*totalSize;		    
            return totalAmount;
		}
		++itr;
	}
    return totalAmount;
}

Pricer::~Pricer () {}

void Pricer::onChange (BookMsg& msg)
{
	double amount = orderBook_.getTotal(targetSize_,  msg.side_);
	
	std::string output(msg.timestamp_);
	
	output = (msg.side_ == 'B' ? 
	                 output + " S " :
	                 output + " B ");
	                 
	double lastAmnt =
	         (msg.side_ == 'B' ? 
	              sellingIncome_ :
	              buyingExpense_);
	              
	 if (amount != lastAmnt)
	 {
	 		if (amount)
	 		{
	 			std::cout << output
                          << std::fixed
	 			          << std::setprecision(2)  
                          << amount 
	 			          << std::endl;
	 		}
	 		else
	 		{
	 			std::cout <<  output
	 			          << "NA"
	 			          << std::endl;
	 		}
	 		msg.side_ == 'B' ? 
	 		sellingIncome_ = amount :
	 		buyingExpense_ = amount;   
	 }
}

void
StringTokenizer::tokenize(std::string& data, std::vector<std::string>& cont)
{
	size_t pos = 0;
	size_t length = data.length();

	while (pos < length)
	{
		size_t pos1 = data.find_first_of(delim_, pos);
		if (pos1 != std::string::npos)
		{
		    std::string subs = data.substr(pos, pos1-pos);
            cont.push_back(subs);
            // Advance to the next word
            pos = data.find_first_not_of(delim_, pos1);
		}
        else
        {
		   std::string subs = data.substr(pos, length-pos);
           cont.push_back(subs);
           return;
        }
	}
}

void InputProcessor::process()
{
	std::string line;
	while (std::getline(std::cin, line))
	{
	      // Find out the type
	      char type = line [MSG_TYPE_POS];
	      if (type == 'A')
	             processAddOrder (line);
	      else if (type == 'R')
	             processReduceOrder (line);
	      else
	      {
	      	std::cerr << "Error: Invalid Msg Type " << type
                      << std::endl;
	      }
	}
}

void InputProcessor::processAddOrder(std::string& line)
{
	// extract the order details
	std::vector<std::string> fields;
	StringTokenizer st(" ");
	st.tokenize(line, fields);
	AddOrder add;
	add.timestamp_ = fields[0];
	add.orderId_ = fields[2];
    add.type_ = fields[3][0];
    if (add.type_ != 'B' && add.type_ != 'S')
    {
        std::cerr << "Error: Order Type must be B or S " 
                  << std::endl;
        return;
    }
    // The following should possibly be validated for correctness
    // In this instance will assume from a good source
    add.price_ = atof(fields[4].c_str());
    add.size_ = atoi(fields[5].c_str());

    // Ensure that above is correct
    // consider changing type to return an error
    orderBook_.addOrder(add);
}

void InputProcessor::processReduceOrder(std::string& line)
{
	std::vector<std::string> fields;
	StringTokenizer st(" ");
	st.tokenize(line, fields);
	ReduceOrder reduce;
	reduce.timestamp_ = fields[0];
	reduce.orderId_ = fields[2];
    reduce.size_ = atoi(fields[3].c_str());
    orderBook_.reduceOrder(reduce);
}


int main (int argc, char *argv [])
{
	if (argc != 2)
	{
		std::cout << "pricer <targetSize> < input\n";
		return -1;
	}
	
	uint32_t targetSize = atol (argv [1]);
	if (targetSize == 0)
	{
		std::cout << "target size should be  a positive integer\n";
		return -1;
	}
	Observer<BookMsg> *ob = new Observer<BookMsg>();
	
	OrderBook orderBook(ob);
	
	InputProcessor input (orderBook);
	
	Pricer pricer (ob, orderBook,
	               targetSize);
	
	input.process ();

	return 0;
}
