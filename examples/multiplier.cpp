#include "flow.h"
#include "samples/generic.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>

using namespace std;

// This class takes its inputs (in terms of T), multiplies them using *=, then outputs the multiplication expression including the product as a string.
// For example, given the inputs of 3 and 4, it outputs the string "3 * 4 = 12".
template<typename T>
class multiplication_expressifier : public flow::transformer<T, string>
{
public:
	multiplication_expressifier(size_t ins = 2, const string& name_r = "multiplication_expressifier") : flow::node(name_r), flow::transformer<T, string>(name_r, ins, 1) {}

	virtual ~multiplication_expressifier() {}

	virtual void ready(size_t n)
	{
		// Check that a packet is ready at all inputs. If not, return and try again when another packet arrives.
		for(size_t i = 0; i != flow::consumer<T>::ins(); ++i)
		{
			if(!flow::consumer<T>::input(i).peek())
			{
				return;
			}
		}

		// Gather the terms in a container.
		vector<unique_ptr<flow::packet<T>>> terms;

		for(size_t i = 0; i != flow::consumer<T>::ins(); ++i)
		{
			terms.emplace_back(move(flow::consumer<T>::input(i).pop()));
		}

		// Start the product as equal to the first term.
		T product(terms[0]->data());

		// Multiply by the value of all other packets.
		for_each(terms.begin() + 1, terms.end(), [&product](const unique_ptr<flow::packet<T>>& packet_up_r){
			product *= packet_up_r->data();
		});

		// Using a stringstream, aggregate all the factors and the product to form the multiplication expression.
		stringstream ss;
		ss << terms[0]->data();
		for_each(terms.begin() + 1, terms.end(), [&ss](const unique_ptr<flow::packet<T>>& packet_up_r){
			ss << " * " << packet_up_r->data();
		});
		ss << " = " << product;

		// ss now looks like "a * b [* x] = p".

		// Make a packet with the expression.
		unique_ptr<flow::packet<string>> p(new flow::packet<string>(ss.str()));

		// Output it.
		flow::producer<string>::output(0).push(p);
	}
};

int main()
{
	// Create a timer that will fire every three seconds.
	flow::monotonous_timer mt(chrono::seconds(3));

	// Instantiate a graph. It starts out empty.
	flow::graph g;

	// Instantiate a random number generator with a uniform distribution of the number 0 to 10.
	random_device rd;
	default_random_engine engine(rd());
	uniform_int_distribution<size_t> uniform(0, 10);
	auto generator = bind(uniform, ref(engine));

	// Create three generators and add them to the graph.
	g.add(make_shared<flow::samples::generic::generator<int>>(mt, generator), "g1");
	g.add(make_shared<flow::samples::generic::generator<int>>(mt, generator), "g2");
	g.add(make_shared<flow::samples::generic::generator<int>>(mt, generator), "g3");
	
	// Include a multiplication_expressifier with three inputs.
	// We specify its inputs to be ints, but its output will always be a string.
	g.add(make_shared<multiplication_expressifier<int>>(3), "me1");

	// Include a consumer that just prints the data packets to cout.
	g.add(make_shared<flow::samples::generic::ostreamer<string>>(cout), "o1");

	// Connect the three generators to the multiplication_expressifier.
	g.connect<int>("g1", 0, "me1", 0);
	g.connect<int>("g2", 0, "me1", 1);
	g.connect<int>("g3", 0, "me1", 2);

	// Connect the multiplication_expressifier to the ostreamer.
	g.connect<string>("me1", 0, "o1", 0);

	// Start the timer on its own thread so it doesn't block us here.
	thread mt_t(ref(mt));

	// Start the graph! Now we should see multiplication expressions printed to the standard output.
	g.start();

	// Wait for some input. And when we get it...
	char c;
	cin >> c;

	// ...stop the graph completely.
	g.stop();

	// Stop the timer.
	mt.stop();
	mt_t.join();

	return 0;
}
