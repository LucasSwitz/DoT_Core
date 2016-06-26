#include <string.h>
#include <stdlib.h>

#define HEARTBEAT_HEADER 0
#define HEARTBEAT_REQUEST 0x01

#define HANDSHAKE_HEADER 1
#define HANDSHAKE_REQUEST 0x11
#define HANDSHAKE_RETURN 0x12

#define SUBSCRIPTION_HEADER 3
#define SUBSCRIPTION_UPDATE 0x31
#define SUBSCRIPTION_INTERRUPT 0x32

#define UNI_DELIM ','

template<class c>
class Vector
{
public:
	Vector(int capacity = 5)
	{
		_capacity = capacity;
		_size = 0;
		_data = (c *)malloc(capacity*sizeof(c));;
	}
	~Vector()
	{
		if (_data)
		{
			free(_data);
			_data = NULL;
		}
	}

	void put(c element)
	{
		_data[_size] = element;
		_size++;

		if (_size == _capacity)
		{
			resize();
		}
	}

	c* asArray()
	{
		c* out = new c[_size];

		for (int i = 0; i < _size; i++)
		{
			out[i] = _data[i];
		}

		return out;
	}

	void resize()
	{
		_capacity *= 2;
		_data = (c *)realloc(_data, sizeof(c)*_capacity);
	}

	int size()
	{
		return _size;
	}

	c get(int i)
	{
		return _data[i];
	}

private:
	c* _data;
	int _capacity;
	int _size = 0;
};

template<class c>
class LinkedList
{
public:

	~LinkedList()
	{
		delete begin;
	}

	template<class d>
	struct node
	{
		~node()
		{
			delete next;
		}
		d data;
		node* next = nullptr;
		int index;
	};

	node<c>* begin = nullptr;
	node<c>* last = nullptr;

	int index = 0;

	void push(c data)
	{
		node<c>* link = (struct node<c>*) malloc(sizeof(struct node<c>));

		if (!last)
			last = link;

		link->data = data;
		link->next = begin;
		link->index = index;
		index++;

		begin = link;
	}

	void flush()
	{
		node<c>* current = begin;

		while (current->next)
		{
			current = current->next;
			delete current;
		}
	}
};

class SubscriptionsDirectory
{
public:
	struct subscription
	{
		struct subscription_description
		{

			enum SubscriptionType
			{
				INT,
				CHAR,
				STRING,
				BYTE_PTR
			};

			subscription_description(const char * key, SubscriptionType t)
			{
				this->type = t;
				this->key = key;
			}

			subscription_description() {};
			SubscriptionType type;
			const char * key;
		};
		~subscription()
		{

		}

		subscription() {};
		subscription(subscription_description d)
		{
			this->desc = d;
		}
		subscription(subscription_description d, unsigned char *data, int size)
		{
			this->val = data;
			this->size = size;
			this->desc = d;
		}

		int valAsInt()
		{
			switch (size)
			{
			case 0:
				return 0;
				break;
			case 1:
				return val[0];
				break;
			case 2:
				return val[0] << 8 | val[1];
				break;
			case 3:
				return val[0] << 16 | val[1] << 8 | val[2];
			default:
				return  val[0] << 24 | val[1] << 16 | val[2] << 8 | val[3];
			}
		}

		int valAsChar()
		{
			return val[0];
		}

		const char * getKey()
		{
			return desc.key;
		}

		subscription_description getDescription()
		{
			return desc;
		}

		unsigned char* val;
		subscription_description desc;
		int size;
	};

	SubscriptionsDirectory(int capacity = 8)
	{
		_capacity = capacity;
		_subscriptions = new LinkedList<subscription*>*[capacity];

		for (int i = 0; i < _capacity; i++)
		{
			_subscriptions[i] = nullptr;
		}
	};

	virtual ~SubscriptionsDirectory()
	{
		delete[] _subscriptions;
	};

	void update(const char * key, unsigned char * val, int size)
	{

		if (this->get(key))
		{
			unsigned char * oldVal = this->get(key)->val;
			int oldSize = this->get(key)->size;

			delete[] this->get(key)->val;
			this->get(key)->val = new unsigned char[size];

			for (int i = 0; i < size; i++)
			{
				this->get(key)->val[i] = val[i];
			}

			this->get(key)->size = size;
		}
	}

	void put(subscription t, unsigned char * val, int numOfBytes)
	{
		if (!_subscriptions[hashKey(t.getDescription().key)])
		{
			_subscriptions[hashKey(t.getDescription().key)] = new LinkedList<subscription*>();
		}

		_subscriptions[hashKey(t.getDescription().key)]->push(new subscription(t.getDescription(), val, numOfBytes));
		_size++;

		if ((double)_size / (double)_capacity >= LOAD_THRESHOLD)
		{
			resize();
		}
	};

	/* Runs in 0(1) worst time if there are no collisions.
	Runs in 0(n) worst time if there are collisions (where n is the number of collided objects per hash)
	*/
	subscription* get(const char * command)
	{
		if (!_subscriptions[hashKey(command)])
		{
			return nullptr;
		}

		LinkedList<subscription*>::node<subscription*>* currentNode = _subscriptions[hashKey(command)]->begin;
		while (currentNode)
		{
			if (!currentNode->next)
				return currentNode->data;

			if (!strcmp(currentNode->data->getKey(), command))
			{
				return currentNode->data;
			}
			currentNode = currentNode->next;
		}
		return nullptr;
	};
private:
	const float LOAD_THRESHOLD = 0.7;
	void resize()
	{
		LinkedList<subscription*>** _subscriptionsOld = _subscriptions;
		int oldBacksSize = _capacity;

		_capacity *= 2;
		_size = 0;
		_subscriptions = new LinkedList<subscription*>*[_capacity];

		for (int i = 0; i < _capacity; i++)
		{
			_subscriptions[i] = nullptr;
		}

		for (int i = 0; i < oldBacksSize; i++)
		{
			if (_subscriptionsOld[i])
			{
				LinkedList<subscription*>::node<subscription*>* currentNode = _subscriptionsOld[i]->begin;
				while (currentNode)
				{
					put(currentNode->data->getDescription(), currentNode->data->val, currentNode->data->size);
					currentNode = currentNode->next;
				}
			}
		}

		for (int i = 0; i < oldBacksSize; i++)
		{
			delete _subscriptionsOld[i];
		}
		delete[] _subscriptionsOld;
	}

	int hashKey(const char * key)
	{
		int sum = 0;
		for (int i = 0; i < strlen(key); i++)
		{
			sum += key[i];
		}
		return sum % _capacity;
	}
	int _size = 0;
	int _capacity;

	LinkedList<subscription*>** _subscriptions;
};

class IoTDevice
{
public:
	typedef SubscriptionsDirectory::subscription::subscription_description sub_desc;
	typedef SubscriptionsDirectory::subscription::subscription_description::SubscriptionType sub_type;

	struct device_description
	{
		~device_description()
		{
			if (token)
				delete token;

			delete[] subscriptions;
		}

		device_description(const char * token, int heartbeatInterval, SubscriptionsDirectory::subscription::subscription_description* subscription_descs, int numberOfSubscriptions)
		{
			this->token = token;
			this->heartbeatInterval = heartbeatInterval;
			this->subscriptions = new SubscriptionsDirectory::subscription[numberOfSubscriptions];

			for (int i = 0; i < numberOfSubscriptions; i++)
			{
				this->subscriptions[i] = SubscriptionsDirectory::subscription(subscription_descs[i]);
			}
			this->numberOfSubscriptions = numberOfSubscriptions;
		};



		void asHandshakeData(Vector<unsigned char>& out)
		{
			//add header
			//add token
			for (int i = 0; i < strlen(token); i++)
			{
				out.put(token[i]);
			}
			out.put(UNI_DELIM);
			//add heatbeatInterval
			out.put((heartbeatInterval >> 24) & 0xFF);

			out.put((heartbeatInterval >> 16) & 0xFF);

			out.put((heartbeatInterval >> 8) & 0xFF);

			out.put(heartbeatInterval & 0xFF);
			out.put(UNI_DELIM);
			//add subscriptions

			for (int i = 0; i < numberOfSubscriptions; i++)
			{
				for (int k = 0; k < strlen(subscriptions[i].getDescription().key); k++)
				{
					out.put(subscriptions[i].getDescription().key[k]);
				}
				out.put(UNI_DELIM);

				switch (subscriptions[i].getDescription().type)
				{
				case sub_type::INT:
					out.put(sub_type::INT);
					break;
				case  sub_type::CHAR:
					out.put(sub_type::CHAR);
					break;
				case  sub_type::STRING:
					out.put(sub_type::STRING);
					break;
				case sub_type::BYTE_PTR:
					out.put(sub_type::BYTE_PTR);
					break;
				}

				out.put(UNI_DELIM);
			}
		}

		const char * token;
		SubscriptionsDirectory::subscription* subscriptions;
		int numberOfSubscriptions;
		int heartbeatInterval;
	};

	IoTDevice(const char * token, int heartbeatInterval, SubscriptionsDirectory::subscription::subscription_description* subscriptions, int numberOfSubscriptions)
		:desc(token, heartbeatInterval, subscriptions, numberOfSubscriptions)
	{
		for (int i = 0; i < desc.numberOfSubscriptions; i++)
		{
			directory.put(desc.subscriptions[i], 0, 1);
		}
	}

	void updateValue(const char * key, int val)
	{
		unsigned char out[4];
		out[0] = (val >> 24) & 0xFF;
		out[1] = (val >> 16) & 0xFF;
		out[2] = (val >> 8) & 0xFF;
		out[3] = val & 0xFF;

		updateValue(key, out, 4);
	}
	void updateValue(const char * key, char val)
	{
		updateValue(key, (unsigned char *)val, 1);
	}
	void updateValue(const char * key, const char * val)
	{
		unsigned char* out = new unsigned char[strlen(val)];
		for (int i = 0; i < strlen(val); i++)
		{
			out[i] = (unsigned char)val[i];
		}
		updateValue(key, out, strlen(val));
	}

	void updateValue(const char * key, const unsigned char * val, int valSize)
	{
		if (true)
		{
			Vector<unsigned char> data;
			for (int i = 0; i < strlen(key); i++)
			{
				data.put(key[i]);
			}
			data.put(UNI_DELIM);

			for (int i = 0; i < valSize; i++)
			{
				data.put(val[i]);
			}

			Vector<unsigned char> out;
			package(out, SUBSCRIPTION_UPDATE, data.asArray(), data.size());

			send(out.asArray(), out.size());
		}
	}

	SubscriptionsDirectory::subscription* getSubscription(const char * key)
	{
		return directory.get(key);
	}

	void onSend(void(*sendFunction)(const unsigned char *, int))
	{
		_sendFunction = sendFunction;
	}

	void send(const unsigned char * out, int size)
	{
		if (_sendFunction)
			_sendFunction(out, size);

		delete[] out;
	}

	void feed(unsigned char * bytesIn, int numOfBytes)
	{
		parseData(bytesIn, numOfBytes);
	}
private:
	device_description desc;
	SubscriptionsDirectory directory;
	bool _verified = false;
	void(*_sendFunction)(const unsigned char *, int);

	void parseData(unsigned char * data, int numOfBytes)
	{
		if (!(numOfBytes > 0))
			return;

		switch (data[0] / 16)
		{
		case SUBSCRIPTION_HEADER:
			switch (data[0])
			{
			case SUBSCRIPTION_UPDATE:
				SubscriptionsDirectory::subscription sub;
				subscriptonFromBytes(data, numOfBytes, sub);
				directory.update(sub.getKey(), sub.val, sub.size);
				break;
			}
			break;
		case HANDSHAKE_HEADER:
			switch (data[0])
			{
			case HANDSHAKE_REQUEST:
				_verified = false;
				handshake();
				break;
			case HANDSHAKE_RETURN:
				_verified = true;
				break;
			}
			break;
		case HEARTBEAT_HEADER:
			break;
		}
	}
	void package(Vector<unsigned char>& out, unsigned char header, const unsigned char* data, int dataSize)
	{
		out.put(header);
		out.put(UNI_DELIM);

		for (int i = 0; i < dataSize; i++)
		{
			out.put(data[i]);
		}
		out.put((char)10);
		out.put((char)13);

		delete[] data;
	}

	void handshake()
	{
		Vector<unsigned char> in;
		Vector<unsigned char> out;
		desc.asHandshakeData(in);
		package(out, HANDSHAKE_RETURN, in.asArray(), in.size());
		send(out.asArray(), out.size());
	}

	void subscriptonFromBytes(unsigned char* data, int numOfBytes, SubscriptionsDirectory::subscription& sub)
	{
		int headerIndex = 1;

		//find where the key ends
		while ((char)data[headerIndex] != UNI_DELIM)
		{
			headerIndex++;
		}

		char* key = (char*)malloc(headerIndex);
		int valueSize = numOfBytes - headerIndex - 3;
		unsigned char* val = (unsigned char *)malloc(valueSize);

		//parse key
		for (int i = 0; i < headerIndex - 1; i++)
		{
			key[i] = (char)data[i + 1];
		}
		key[headerIndex - 1] = (char)0;

		//parse value
		int k = 0;
		for (int i = headerIndex + 1; i < headerIndex + valueSize + 1; i++)
		{
			val[k] = data[i];
			k++;
		}

		sub.desc.key = key;
		sub.size = valueSize;
		sub.val = val;
		//this might break
	}
};

