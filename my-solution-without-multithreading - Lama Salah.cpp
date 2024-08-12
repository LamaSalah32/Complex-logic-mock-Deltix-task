#include <bits/stdc++.h>

typedef long long ll;
using namespace std;

const int USER_TABLE_SIZE = 557; // Size of the user hash table
const int CURRENCY_TABLE_SIZE = 13; // Size of the currency hash table
const vector<pair<ll, string>> PERIODS = {{3600, "bars-1h.csv"}, {86400, "bars-1d.csv"}, {2592000, "bars-30d.csv"}}; // Time periods for aggregation
set <ll> allTimestamps; // Set to store all unique timestamps
ll startTime = 1e15; // Start time for the dataset
ll endTime = -1e15; // End time for the dataset

struct TransactionSchema{
    ll timestamp;
    int currencyId;
    long double transactionAmount;
};

struct BarData{
    ll timestamp;
    long double min_balance;
    long double max_balance;
    long double avg_balance;
};

class Transactions{
    string key; 
    vector<TransactionSchema> transactions; 

public:
    Transactions() : key("") {}

    Transactions(const string k, ll time, int currency, double transactionAmount) : key(k){
        transactions.push_back({time, currency, transactionAmount});
    }

    // Add a new transaction
    void addTimestamp(ll time, int currency, double transactionAmount){
        transactions.push_back({time, currency, transactionAmount});
    }

    // Get transaction delta at a specific time
    pair<int, double> getTransactionDelta(ll time){
        if (transactions.empty())
            return {-1, 0};

        int l = 0, r = transactions.size() - 1;
        while (l <= r){
            int mid = l + (r - l) / 2;
            if (transactions[mid].timestamp == time)
                return {transactions[mid].currencyId, transactions[mid].transactionAmount};
            else if (transactions[mid].timestamp < time)
                l = mid + 1;
            else
                r = mid - 1;
        }

        return {-1, 0};
    }

    // Get the transaction price at a specific time
    double getTransactionPrice(ll time){
        if (transactions.empty())
            return 0;

        int l = 0, r = transactions.size();
        while (l < r - 1){
            int mid = l + (r - l) / 2;
            if (transactions[mid].timestamp <= time)
                l = mid;
            else
                r = mid;
        }

        return transactions[l].timestamp <= time? transactions[l].transactionAmount : 0;
    }

    const string &get_key() const{
        return key;
    }
};

class HashTable{
    ll size;
    const ll prime = 31;
    vector<Transactions> table;

    // Hash function to compute the index for a given key
    int hash_function(const string &key){
        ll hash = 0;
        for (char ch : key){
            hash = (hash * prime + ch) % size;
        }
        return hash;
    }

public:
    HashTable(int table_size) : size(table_size), table(table_size) {}

    // Insert a transaction into the hash table
    void insert(const string &key, ll time, int currency, double delta){
        int index = find(key);
        if (index != -1){
            table[index].addTimestamp(time, currency, delta);
            return;
        }

        index = hash_function(key);
        int x = index, i = 1;
        while (!table[x].get_key().empty()){
            x = (index + i * i) % size;
            i++;
        }

        table[x] = Transactions(key, time, currency, delta);
    }

    // Find the index of a key in the hash table
    int find(const string &key){
        int index = hash_function(key);
        int x = index, i = 1;
        while (!table[x].get_key().empty() && table[x].get_key() != key){
            x = (index + i * i) % size;
            i++;
        }
        return table[x].get_key() == key ? x : -1;
    }

    // Get the transaction delta at a specific time
    pair<int, double> getTransactionDelta(int x, ll time){
        return table[x].getTransactionDelta(time);
    }

    // Get the transaction price at a specific time
    double getTransactionPrice(int x, ll time){
        return table[x].getTransactionPrice(time);
    }

    // Get the key at a specific index
    string get_key(int idx){
        return table[idx].get_key();
    }
};

// Function to read market data from a file
void readMarketData(const string filename, HashTable &currencyTable){
    ifstream market_file(filename);
    string line;

    // Insert a default entry for USD
    currencyTable.insert("USD", 0, 0, 1);

    getline(market_file, line); // Skip header line
    while (getline(market_file, line)){
        line += ','; // Append comma to handle last cell
        vector<string> tokens(3);
        string cell;
        for (int i = 0, idx = 0; i < line.size(); i++){
            if (line[i] == ','){
                tokens[idx++] = cell;
                cell.clear();
                continue;
            }
            cell += line[i];
        }

        // Insert market data into the market table
        currencyTable.insert(tokens[0].substr(0, 3), stoll(tokens[1]), 0, stod(tokens[2]));
        allTimestamps.insert(stoll(tokens[1])); // Add timestamp to the set
    }

    market_file.close();
}

// Function to read user data from a file
void readUserData(const string  filename, HashTable &userTable, HashTable &currencyTable){
    ifstream user_file(filename);
    string line;

    getline(user_file, line); // Skip header line
    while (getline(user_file, line)){
        line += ','; // Append comma to handle last cell
        vector<string> tokens(4);
        string cell;
        for (int i = 0, idx = 0; i < line.size(); i++){
            if (line[i] == ','){
                tokens[idx++] = cell;
                cell.clear();
                continue;
            }
            cell += line[i];
        }

        ll time = stoll(tokens[2]);
        userTable.insert(tokens[0], time, currencyTable.find(tokens[1]), stod(tokens[3])); // Insert user data into the user table
        allTimestamps.insert(time); // Add timestamp to the set
        startTime = min(startTime, time); // Update start time
        endTime = max(endTime, time); // Update end time
    }

    user_file.close();
}

// Function to aggregate data and output to a CSV file
void aggregateData(ll period, HashTable  userTable, HashTable currencyTable, const string  output_file_name){
    ofstream output_file(output_file_name);
    output_file << "user_id,minimum_balance,maximum_balance,average_balance,start_timestamp\n";

    // Iterate over all users in the hash table
    for (int u = 0; u < USER_TABLE_SIZE; u++){
        if (userTable.get_key(u).empty())
            continue;

        BarData bar = {0, 0, 0, 0};
        vector<vector<long double>> user_balances(CURRENCY_TABLE_SIZE);
        double lastPrice = 0;

        // Iterate over all timestamps
        for (auto t = allTimestamps.begin(); t != allTimestamps.end(); t++){
            if (*t < startTime/period*period || *t > endTime/period*period + period) continue;

            ll curr_period = *t / period * period;
            long double price = 0;

            // Get transaction delta for the current timestamp
            auto [curr, delta] = userTable.getTransactionDelta(u, *t);

            if (curr != -1){
                long double new_delta = (user_balances[curr].empty() ? 0 : user_balances[curr].back()) + delta;
                user_balances[curr].emplace_back(new_delta);
            }

            // Calculate the total balance in USD
            for (int c = 0; c < CURRENCY_TABLE_SIZE; c++){
                if (currencyTable.get_key(c).empty())
                    continue;

                long double balance = user_balances[c].empty() ? 0 : user_balances[c].back();
                price += balance * currencyTable.getTransactionPrice(c, *t);
            }

            // Handle new period
            if (bar.timestamp != curr_period){
                if (bar.timestamp != 0){
                    output_file << userTable.get_key(u) << "," << fixed << setprecision(4)
                        << bar.min_balance << "," << bar.max_balance << ","
                        << bar.avg_balance/period << "," << bar.timestamp << "\n";
                }

                bar = {curr_period, 1e15, -1e15, 0};
                bar.avg_balance += abs(*t - curr_period) * lastPrice;
            }

            // Calculate average balance for the period
            auto next = std::next(t);
            if (next != allTimestamps.end() && *next / period * period == curr_period){
                bar.avg_balance += abs(*next - *t) * price;
            }
            else{
                bar.avg_balance += abs((curr_period + period) - *t) * price;
            }

            lastPrice = price;

            // Update min and max balances
            bar.min_balance = min(bar.min_balance, price);
            bar.max_balance = max(bar.max_balance, price);
        }

        output_file << userTable.get_key(u) << "," << fixed << setprecision(4)
                        << bar.min_balance << "," << bar.max_balance << ","
                        << bar.avg_balance/period << "," << bar.timestamp << "\n";
    }

    output_file.close();
}

int main(){
    HashTable userTable(USER_TABLE_SIZE);
    HashTable currencyTable(CURRENCY_TABLE_SIZE);

    readMarketData("market_data.csv", currencyTable); // Read market data
    readUserData("user_data.csv", userTable, currencyTable); // Read user data

    // Aggregate data for different periods and write to CSV files
    for (auto& [p, file] : PERIODS){
        aggregateData(p, userTable, currencyTable, file);
    }

    return 0;
}
