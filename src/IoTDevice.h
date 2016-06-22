#include <string.h>
#include <stdlib.h>

#define HEARTBEAT_HEADER 0
#define HEARTBEAT_REQUEST 0x01

#define HANDSHAKE_HEADER 1
#define HANDSHAKE_REQUEST 0x11
#define HANDSHAKE_RETURN 0x12

#define SUBSCRIPTION_HEADER 3
#define SUBSCRIPTION_UPDATE 0x31

#define UNI_DELIM ','


template<class c>
class Vector
{
public:
	Vector(int capacity = 5)
	{
		_capacity = capacity;
		_data = new c[_capacity];
	}
	~Vector()
	{
		//delete[] _data;
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
		return _data;
	}

	void resize()
	{
		c* oldData = _data;
		int oldSize = _size;
		_capacity *= 2;
		_size = 0;
		_data = new c[_capacity];

		for (int i = 0; i < oldSize; i++)
		{
			put(oldData[i]);
		}

		delete[] oldData;
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

		~subscription()
		{
			delete key;
			delete val;
		}

		subscription() {};
		subscription(const char * key, unsigned char *data, int size)
		{
			this->key = key;
			this->val = data;
		}

		int valAsInt()
		{
			return  val[0] << 24 | val[1] << 16 | val[2] << 8 | val[3];
		}

		int valAsChar()
		{
			return val[0];
		}

		const char* key;
		unsigned char* val;
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
			this->get(key)->val = val;
			this->get(key)->size = size;
		}
		else
		{
			this->put(key, val, size);
		}
	}

	void put(const char * command, unsigned char * val, int numOfBytes)
	{
		if (!_subscriptions[hashKey(command)])
		{
			_subscriptions[hashKey(command)] = new LinkedList<subscription*>();
		}

		_subscriptions[hashKey(command)]->push(new subscription(command, val, numOfBytes));
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

			if (!strcmp(currentNode->data->key, command))
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
					put(currentNode->data->key, currentNode->data->val, currentNode->data->size);
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

	struct device_description
	{
		~device_description()
		{
			delete token;

			for (int i = 0; i < numberOfSubscriptions; i++)
			{
				delete subscriptions[i];
			}

			delete[] subscriptions;
		}
		device_description(const char * token, int heartbeatInterval, const char** subscriptions, int numberOfSubscriptions)
		{
			this->token = token;
			this->heartbeatInterval = heartbeatInterval;
			this->subscriptions = subscriptions;
			this->numberOfSubscriptions = numberOfSubscriptions;
		};

		Vector<unsigned char> asHandshakeData()
		{

			Vector<unsigned char> out;

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
				for (int k = 0; k < strlen(subscriptions[i]); k++)
				{
					out.put(subscriptions[i][k]);
				}
				out.put(UNI_DELIM);
			}
			return out;
		}

		const char * token;
		const char ** subscriptions;
		int numberOfSubscriptions;
		int heartbeatInterval;
	};

	IoTDevice(const char * token, int heartbeatInterval, const char** subscriptions, int numberOfSubscriptions)
		:desc(token, heartbeatInterval, subscriptions, numberOfSubscriptions)
	{
		for (int i = 0; i < desc.numberOfSubscriptions; i++)
		{
			directory.put(desc.subscriptions[i], NULL, 0);
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
		if (_verified)
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

			Vector<unsigned char> out = package(SUBSCRIPTION_UPDATE, data.asArray(), data.size());
			send(out.asArray(), out.size());
		}
	}

	SubscriptionsDirectory::subscription getSubscription(const char * key)
	{
		directory.get(key);
	}

	void onSend(void(*sendFunction)(const unsigned char *, int))
	{
		_sendFunction = sendFunction;
	}

	void send(const unsigned char * out, int size)
	{
		if (_sendFunction)
			_sendFunction(out, size);

		free((unsigned char *)out);
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
				sub = subscriptonFromBytes(data, numOfBytes);
				directory.update(sub.key, sub.val, sub.size);
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
	const Vector<unsigned char> package(unsigned char header, const unsigned char* data, int dataSize)
	{
		Vector<unsigned char> out;
		out.put(header);
		out.put(UNI_DELIM);

		for (int i = 0; i < dataSize; i++)
		{
			out.put(data[i]);
		}
		out.put((char)10);
		out.put((char)13);

		free((unsigned char *)data);
		return out;
	}

	void handshake()
	{
		Vector<unsigned char >out = package(HANDSHAKE_RETURN, desc.asHandshakeData().asArray(), desc.asHandshakeData().size());
		send(out.asArray(), out.size());	
	}

	SubscriptionsDirectory::subscription& subscriptonFromBytes(unsigned char* data, int numOfBytes)
	{
		SubscriptionsDirectory::subscription out;
		int headerIndex = 1;

		//find where the key ends
		while ((char)data[headerIndex] != UNI_DELIM)
		{
			headerIndex++;
		}

		char* key = (char*)malloc(headerIndex);
		int valueSize = numOfBytes - headerIndex - 1;
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

		out.key = key;
		out.size = valueSize;
		out.val = val;

		//this might break
		free((unsigned char *)data);
		return out;
	}
};
