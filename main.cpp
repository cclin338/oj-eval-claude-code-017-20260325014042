#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace std;

// Constants
const int MAX_USERS = 10000;
const int MAX_TRAINS = 10000;
const int MAX_STATIONS = 100;
const int MAX_ORDERS = 100000;
const int MAX_PENDING_ORDERS = 100000;
const int MAX_DAYS = 92;  // June, July, August 2021

// Date utilities
int dateToDay(const string& date) {
    int month = (date[0] - '0') * 10 + (date[1] - '0');
    int day = (date[3] - '0') * 10 + (date[4] - '0');
    if (month == 6) return day;
    if (month == 7) return 30 + day;
    return 61 + day;  // August
}

string dayToDate(int day) {
    if (day <= 30) {
        char buf[6];
        sprintf(buf, "06-%02d", day);
        return string(buf);
    } else if (day <= 61) {
        char buf[6];
        sprintf(buf, "07-%02d", day - 30);
        return string(buf);
    } else {
        char buf[6];
        sprintf(buf, "08-%02d", day - 61);
        return string(buf);
    }
}

// Time utilities
int timeToMinutes(const string& time) {
    int hour = (time[0] - '0') * 10 + (time[1] - '0');
    int minute = (time[3] - '0') * 10 + (time[4] - '0');
    return hour * 60 + minute;
}

string minutesToTime(int minutes) {
    minutes %= 24 * 60;
    char buf[6];
    sprintf(buf, "%02d:%02d", minutes / 60, minutes % 60);
    return string(buf);
}

// User structure
struct User {
    char username[21];
    char password[31];
    char name[16];  // Chinese characters, 2-5 chars, up to 15 bytes + null
    char mailAddr[31];
    int privilege;
    bool exists;
    bool online;

    User() {
        username[0] = '\0';
        password[0] = '\0';
        name[0] = '\0';
        mailAddr[0] = '\0';
        privilege = 0;
        exists = false;
        online = false;
    }
};

// Train structure
struct Train {
    char trainID[21];
    int stationNum;
    char stations[MAX_STATIONS][31];  // Chinese characters
    int seatNum;
    int prices[MAX_STATIONS - 1];
    int startTime;  // in minutes
    int travelTimes[MAX_STATIONS - 1];
    int stopoverTimes[MAX_STATIONS - 2];
    int saleDateStart;  // day number
    int saleDateEnd;    // day number
    char type;
    bool released;
    bool exists;

    Train() {
        trainID[0] = '\0';
        stationNum = 0;
        seatNum = 0;
        startTime = 0;
        type = ' ';
        released = false;
        exists = false;
        for (int i = 0; i < MAX_STATIONS; i++) {
            stations[i][0] = '\0';
        }
        for (int i = 0; i < MAX_STATIONS - 1; i++) {
            prices[i] = 0;
            travelTimes[i] = 0;
        }
        for (int i = 0; i < MAX_STATIONS - 2; i++) {
            stopoverTimes[i] = 0;
        }
    }
};

// Seat availability tracking
struct SeatManager {
    int*** seats;  // Dynamic 3D array: seats[train][day][segment]
    bool* initialized;  // Track which trains are initialized
    int* stationNums;  // Store station count for each train
    int* seatNums;  // Store seat count for each train
    int* dayOffsets;  // Store day offset for each train (to map actual day to index)
    int* numDaysArray;  // Store number of days allocated for each train

    SeatManager() {
        seats = new int**[MAX_TRAINS];
        initialized = new bool[MAX_TRAINS];
        stationNums = new int[MAX_TRAINS];
        seatNums = new int[MAX_TRAINS];
        dayOffsets = new int[MAX_TRAINS];
        numDaysArray = new int[MAX_TRAINS];
        for (int i = 0; i < MAX_TRAINS; i++) {
            seats[i] = nullptr;
            initialized[i] = false;
            stationNums[i] = 0;
            seatNums[i] = 0;
            dayOffsets[i] = 0;
            numDaysArray[i] = 0;
        }
    }

    ~SeatManager() {
        for (int i = 0; i < MAX_TRAINS; i++) {
            if (seats[i] != nullptr) {
                for (int j = 0; j < numDaysArray[i]; j++) {
                    if (seats[i][j] != nullptr) {
                        delete[] seats[i][j];
                    }
                }
                delete[] seats[i];
            }
        }
        delete[] seats;
        delete[] initialized;
        delete[] stationNums;
        delete[] seatNums;
        delete[] dayOffsets;
        delete[] numDaysArray;
    }

    void initTrain(int trainIdx, int stationNum, int seatNum, int saleDateStart, int saleDateEnd) {
        if (seats[trainIdx] != nullptr) {
            // Already initialized, clear it
            for (int j = 0; j < numDaysArray[trainIdx]; j++) {
                if (seats[trainIdx][j] != nullptr) {
                    delete[] seats[trainIdx][j];
                }
            }
            delete[] seats[trainIdx];
        }

        int numDays = saleDateEnd - saleDateStart + 1;
        seats[trainIdx] = new int*[numDays];
        for (int day = 0; day < numDays; day++) {
            seats[trainIdx][day] = new int[stationNum - 1];
            for (int i = 0; i < stationNum - 1; i++) {
                seats[trainIdx][day][i] = seatNum;
            }
        }
        initialized[trainIdx] = true;
        stationNums[trainIdx] = stationNum;
        seatNums[trainIdx] = seatNum;
        dayOffsets[trainIdx] = saleDateStart;
        numDaysArray[trainIdx] = numDays;
    }

    void clearTrain(int trainIdx) {
        if (seats[trainIdx] != nullptr) {
            for (int j = 0; j < numDaysArray[trainIdx]; j++) {
                if (seats[trainIdx][j] != nullptr) {
                    delete[] seats[trainIdx][j];
                }
            }
            delete[] seats[trainIdx];
            seats[trainIdx] = nullptr;
        }
        initialized[trainIdx] = false;
        stationNums[trainIdx] = 0;
        seatNums[trainIdx] = 0;
        dayOffsets[trainIdx] = 0;
        numDaysArray[trainIdx] = 0;
    }

    int querySeat(int trainIdx, int day, int startStation, int endStation, int stationNum) {
        if (!initialized[trainIdx]) return 0;
        int dayIdx = day - dayOffsets[trainIdx];
        if (dayIdx < 0 || dayIdx >= numDaysArray[trainIdx]) return 0;
        if (seats[trainIdx][dayIdx] == nullptr) return 0;
        int minSeat = seats[trainIdx][dayIdx][startStation];
        for (int i = startStation; i < endStation; i++) {
            if (seats[trainIdx][dayIdx][i] < minSeat) {
                minSeat = seats[trainIdx][dayIdx][i];
            }
        }
        return minSeat;
    }

    bool buyTickets(int trainIdx, int day, int startStation, int endStation, int num, int stationNum) {
        if (!initialized[trainIdx]) return false;
        int dayIdx = day - dayOffsets[trainIdx];
        if (dayIdx < 0 || dayIdx >= numDaysArray[trainIdx]) return false;
        if (seats[trainIdx][dayIdx] == nullptr) return false;
        // Check if enough seats
        for (int i = startStation; i < endStation; i++) {
            if (seats[trainIdx][dayIdx][i] < num) {
                return false;
            }
        }
        // Decrease seats
        for (int i = startStation; i < endStation; i++) {
            seats[trainIdx][dayIdx][i] -= num;
        }
        return true;
    }

    void refundTickets(int trainIdx, int day, int startStation, int endStation, int num, int stationNum) {
        if (!initialized[trainIdx]) return;
        int dayIdx = day - dayOffsets[trainIdx];
        if (dayIdx < 0 || dayIdx >= numDaysArray[trainIdx]) return;
        if (seats[trainIdx][dayIdx] == nullptr) return;
        for (int i = startStation; i < endStation; i++) {
            seats[trainIdx][dayIdx][i] += num;
        }
    }
};

// Order structure
struct Order {
    char username[21];
    char trainID[21];
    int day;
    int startStation;
    int endStation;
    int num;
    int price;
    int timestamp;
    char status;  // 's' = success, 'p' = pending, 'r' = refunded
    bool exists;

    Order() {
        username[0] = '\0';
        trainID[0] = '\0';
        day = 0;
        startStation = 0;
        endStation = 0;
        num = 0;
        price = 0;
        timestamp = 0;
        status = ' ';
        exists = false;
    }
};

// Global data
User users[MAX_USERS];
Train trains[MAX_TRAINS];
Order orders[MAX_ORDERS];
Order pendingOrders[MAX_PENDING_ORDERS];
SeatManager seatManager;
int userCount = 0;
int trainCount = 0;
int orderCount = 0;
int pendingOrderCount = 0;
int currentTimestamp = 0;

// Hash function for strings
unsigned int hashString(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

// Find user by username
int findUser(const char* username) {
    unsigned int hash = hashString(username);
    for (int i = 0; i < userCount; i++) {
        if (users[i].exists && strcmp(users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

// Find train by trainID
int findTrain(const char* trainID) {
    for (int i = 0; i < trainCount; i++) {
        if (trains[i].exists && strcmp(trains[i].trainID, trainID) == 0) {
            return i;
        }
    }
    return -1;
}

// Find station index in train
int findStation(int trainIdx, const char* station) {
    for (int i = 0; i < trains[trainIdx].stationNum; i++) {
        if (strcmp(trains[trainIdx].stations[i], station) == 0) {
            return i;
        }
    }
    return -1;
}

// Calculate cumulative price
int calculatePrice(int trainIdx, int startStation, int endStation) {
    int price = 0;
    for (int i = startStation; i < endStation; i++) {
        price += trains[trainIdx].prices[i];
    }
    return price;
}

// Calculate arrival and departure times
void calculateTimes(int trainIdx, int day, int stationIdx, int& arriveDay, int& arriveTime, int& departDay, int& departTime) {
    Train& t = trains[trainIdx];

    if (stationIdx == 0) {
        // Starting station
        arriveDay = day;
        arriveTime = -1;  // Invalid
        departDay = day;
        departTime = t.startTime;
    } else if (stationIdx == t.stationNum - 1) {
        // Terminal station
        arriveDay = day;
        arriveTime = t.startTime;
        for (int i = 0; i < stationIdx; i++) {
            arriveTime += t.travelTimes[i];
            if (i < t.stationNum - 2) {
                arriveTime += t.stopoverTimes[i];
            }
        }
        // Adjust for day transitions
        int dayOffset = arriveTime / 1440;
        arriveTime %= 1440;
        arriveDay += dayOffset;
        departDay = arriveDay;
        departTime = -1;  // Invalid
    } else {
        // Intermediate station
        arriveDay = day;
        arriveTime = t.startTime;
        for (int i = 0; i < stationIdx; i++) {
            arriveTime += t.travelTimes[i];
            if (i < t.stationNum - 2) {
                arriveTime += t.stopoverTimes[i];
            }
        }
        // Adjust for day transitions
        int dayOffset = arriveTime / 1440;
        arriveTime %= 1440;
        arriveDay += dayOffset;

        departDay = arriveDay;
        departTime = arriveTime + t.stopoverTimes[stationIdx - 1];
        // Check if departure time crosses into next day
        if (departTime >= 1440) {
            departDay++;
            departTime %= 1440;
        }
    }
}

// Parse command
void parseCommand(const string& cmd, char* key, char* value) {
    int pos = cmd.find(' ');
    if (pos == -1) {
        key[0] = '\0';
        value[0] = '\0';
        return;
    }
    strncpy(key, cmd.substr(0, pos).c_str(), 100);
    strncpy(value, cmd.substr(pos + 1).c_str(), 1000);
}

// Parse parameters
void parseParams(const string& params, char keys[10][2], char values[10][1000], int& paramCount) {
    paramCount = 0;
    int pos = 0;
    while (pos < params.length()) {
        // Skip whitespace
        while (pos < params.length() && params[pos] == ' ') pos++;
        if (pos >= params.length()) break;

        // Find key
        if (params[pos] != '-') break;
        int keyStart = pos + 1;
        pos = params.find(' ', pos);
        if (pos == -1) break;
        keys[paramCount][0] = params[keyStart];
        keys[paramCount][1] = '\0';

        // Skip whitespace
        while (pos < params.length() && params[pos] == ' ') pos++;
        if (pos >= params.length()) break;

        // Find value
        int valueStart = pos;
        pos = params.find(" -", pos);
        if (pos == -1) {
            strncpy(values[paramCount], params.substr(valueStart).c_str(), 1000);
            paramCount++;
            break;
        } else {
            strncpy(values[paramCount], params.substr(valueStart, pos - valueStart).c_str(), 1000);
            paramCount++;
        }
    }
}

// Get parameter value
const char* getParam(char keys[10][2], char values[10][1000], int paramCount, char key) {
    for (int i = 0; i < paramCount; i++) {
        if (keys[i][0] == key) {
            return values[i];
        }
    }
    return nullptr;
}

// Check if parameter exists
bool hasParam(char keys[10][2], char values[10][1000], int paramCount, char key) {
    return getParam(keys, values, paramCount, key) != nullptr;
}

// Command implementations

int add_user(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* cur_username = getParam(keys, values, paramCount, 'c');
    const char* username = getParam(keys, values, paramCount, 'u');
    const char* password = getParam(keys, values, paramCount, 'p');
    const char* name = getParam(keys, values, paramCount, 'n');
    const char* mailAddr = getParam(keys, values, paramCount, 'm');
    const char* privilege_str = getParam(keys, values, paramCount, 'g');

    // Check if first user
    if (userCount == 0) {
        // First user, privilege must be 10, ignore -c and -g
        if (findUser(username) != -1) return -1;  // Username already exists

        users[userCount].exists = true;
        strcpy(users[userCount].username, username);
        strcpy(users[userCount].password, password);
        strcpy(users[userCount].name, name);
        strcpy(users[userCount].mailAddr, mailAddr);
        users[userCount].privilege = 10;
        userCount++;
        return 0;
    } else {
        // Not first user
        int cur_idx = findUser(cur_username);
        if (cur_idx == -1) return -1;  // Current user doesn't exist
        if (!users[cur_idx].online) return -1;  // Current user not logged in

        int privilege = atoi(privilege_str);
        if (privilege >= users[cur_idx].privilege) return -1;  // Privilege too high

        if (findUser(username) != -1) return -1;  // Username already exists

        users[userCount].exists = true;
        strcpy(users[userCount].username, username);
        strcpy(users[userCount].password, password);
        strcpy(users[userCount].name, name);
        strcpy(users[userCount].mailAddr, mailAddr);
        users[userCount].privilege = privilege;
        userCount++;
        return 0;
    }
}

int login(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* username = getParam(keys, values, paramCount, 'u');
    const char* password = getParam(keys, values, paramCount, 'p');

    int idx = findUser(username);
    if (idx == -1) return -1;
    if (users[idx].online) return -1;  // Already logged in
    if (strcmp(users[idx].password, password) != 0) return -1;

    users[idx].online = true;
    return 0;
}

int logout(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* username = getParam(keys, values, paramCount, 'u');

    int idx = findUser(username);
    if (idx == -1) return -1;
    if (!users[idx].online) return -1;

    users[idx].online = false;
    return 0;
}

string query_profile(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* cur_username = getParam(keys, values, paramCount, 'c');
    const char* username = getParam(keys, values, paramCount, 'u');

    int cur_idx = findUser(cur_username);
    if (cur_idx == -1) return "-1";
    if (!users[cur_idx].online) return "-1";

    int idx = findUser(username);
    if (idx == -1) return "-1";

    if (users[cur_idx].privilege <= users[idx].privilege && cur_idx != idx) return "-1";

    char buf[200];
    sprintf(buf, "%s %s %s %d", users[idx].username, users[idx].name, users[idx].mailAddr, users[idx].privilege);
    return string(buf);
}

string modify_profile(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* cur_username = getParam(keys, values, paramCount, 'c');
    const char* username = getParam(keys, values, paramCount, 'u');
    const char* password = getParam(keys, values, paramCount, 'p');
    const char* name = getParam(keys, values, paramCount, 'n');
    const char* mailAddr = getParam(keys, values, paramCount, 'm');
    const char* privilege_str = getParam(keys, values, paramCount, 'g');

    int cur_idx = findUser(cur_username);
    if (cur_idx == -1) return "-1";
    if (!users[cur_idx].online) return "-1";

    int idx = findUser(username);
    if (idx == -1) return "-1";

    if (users[cur_idx].privilege <= users[idx].privilege && cur_idx != idx) return "-1";

    if (privilege_str != nullptr) {
        int privilege = atoi(privilege_str);
        if (privilege >= users[cur_idx].privilege) return "-1";
        users[idx].privilege = privilege;
    }
    if (password != nullptr) strcpy(users[idx].password, password);
    if (name != nullptr) strcpy(users[idx].name, name);
    if (mailAddr != nullptr) strcpy(users[idx].mailAddr, mailAddr);

    char buf[200];
    sprintf(buf, "%s %s %s %d", users[idx].username, users[idx].name, users[idx].mailAddr, users[idx].privilege);
    return string(buf);
}

int add_train(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* trainID = getParam(keys, values, paramCount, 'i');
    const char* stationNum_str = getParam(keys, values, paramCount, 'n');
    const char* seatNum_str = getParam(keys, values, paramCount, 'm');
    const char* stations_str = getParam(keys, values, paramCount, 's');
    const char* prices_str = getParam(keys, values, paramCount, 'p');
    const char* startTime = getParam(keys, values, paramCount, 'x');
    const char* travelTimes_str = getParam(keys, values, paramCount, 't');
    const char* stopoverTimes_str = getParam(keys, values, paramCount, 'o');
    const char* saleDate_str = getParam(keys, values, paramCount, 'd');
    const char* type = getParam(keys, values, paramCount, 'y');

    if (findTrain(trainID) != -1) return -1;  // Train already exists

    int stationNum = atoi(stationNum_str);
    int seatNum = atoi(seatNum_str);

    trains[trainCount].exists = true;
    strcpy(trains[trainCount].trainID, trainID);
    trains[trainCount].stationNum = stationNum;
    trains[trainCount].seatNum = seatNum;
    trains[trainCount].startTime = timeToMinutes(startTime);
    trains[trainCount].type = type[0];

    // Parse stations
    int pos = 0;
    for (int i = 0; i < stationNum; i++) {
        int next = string(stations_str).find('|', pos);
        if (next == -1) next = string(stations_str).length();
        int len = next - pos;
        if (len > 30) len = 30;
        strncpy(trains[trainCount].stations[i], stations_str + pos, len);
        trains[trainCount].stations[i][len] = '\0';  // Null terminate
        pos = next + 1;
    }

    // Parse prices
    pos = 0;
    for (int i = 0; i < stationNum - 1; i++) {
        int next = string(prices_str).find('|', pos);
        if (next == -1) next = string(prices_str).length();
        trains[trainCount].prices[i] = atoi(prices_str + pos);
        pos = next + 1;
    }

    // Parse travelTimes
    pos = 0;
    for (int i = 0; i < stationNum - 1; i++) {
        int next = string(travelTimes_str).find('|', pos);
        if (next == -1) next = string(travelTimes_str).length();
        trains[trainCount].travelTimes[i] = atoi(travelTimes_str + pos);
        pos = next + 1;
    }

    // Parse stopoverTimes
    if (stationNum > 2) {
        pos = 0;
        for (int i = 0; i < stationNum - 2; i++) {
            int next = string(stopoverTimes_str).find('|', pos);
            if (next == -1) next = string(stopoverTimes_str).length();
            trains[trainCount].stopoverTimes[i] = atoi(stopoverTimes_str + pos);
            pos = next + 1;
        }
    }

    // Parse saleDate
    pos = 0;
    int next = string(saleDate_str).find('|', pos);
    trains[trainCount].saleDateStart = dateToDay(saleDate_str);
    trains[trainCount].saleDateEnd = dateToDay(saleDate_str + next + 1);

    // Initialize seats
    seatManager.initTrain(trainCount, stationNum, seatNum, trains[trainCount].saleDateStart, trains[trainCount].saleDateEnd);

    trainCount++;
    return 0;
}

int release_train(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* trainID = getParam(keys, values, paramCount, 'i');

    int idx = findTrain(trainID);
    if (idx == -1) return -1;
    if (trains[idx].released) return -1;

    trains[idx].released = true;
    return 0;
}

string query_train(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* trainID = getParam(keys, values, paramCount, 'i');
    const char* date = getParam(keys, values, paramCount, 'd');

    int idx = findTrain(trainID);
    if (idx == -1) return "-1";

    int day = dateToDay(date);
    if (day < trains[idx].saleDateStart || day > trains[idx].saleDateEnd) return "-1";

    string result = string(trainID) + " " + trains[idx].type + "\n";

    for (int i = 0; i < trains[idx].stationNum; i++) {
        char line[200];
        int arriveDay, arriveTime, departDay, departTime;
        calculateTimes(idx, day, i, arriveDay, arriveTime, departDay, departTime);

        char arriveStr[20], departStr[20];
        if (i == 0) {
            strcpy(arriveStr, "xx-xx xx:xx");
            sprintf(departStr, "%s %s", dayToDate(departDay).c_str(), minutesToTime(departTime).c_str());
        } else if (i == trains[idx].stationNum - 1) {
            sprintf(arriveStr, "%s %s", dayToDate(arriveDay).c_str(), minutesToTime(arriveTime).c_str());
            strcpy(departStr, "xx-xx xx:xx");
        } else {
            sprintf(arriveStr, "%s %s", dayToDate(arriveDay).c_str(), minutesToTime(arriveTime).c_str());
            sprintf(departStr, "%s %s", dayToDate(departDay).c_str(), minutesToTime(departTime).c_str());
        }

        int price = calculatePrice(idx, 0, i);
        int seat;
        if (i == trains[idx].stationNum - 1) {
            seat = -1;  // x
        } else {
            seat = seatManager.querySeat(idx, day, i, i + 1, trains[idx].stationNum);
        }

        if (seat == -1) {
            sprintf(line, "%s %s -> %s %d x", trains[idx].stations[i], arriveStr, departStr, price);
        } else {
            sprintf(line, "%s %s -> %s %d %d", trains[idx].stations[i], arriveStr, departStr, price, seat);
        }
        result += string(line) + "\n";
    }

    return result;
}

int delete_train(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* trainID = getParam(keys, values, paramCount, 'i');

    int idx = findTrain(trainID);
    if (idx == -1) return -1;
    if (trains[idx].released) return -1;

    seatManager.clearTrain(idx);
    trains[idx].exists = false;
    return 0;
}

struct TicketResult {
    char trainID[21];
    char from[31];
    char to[31];
    int departDay;
    int departTime;
    int arriveDay;
    int arriveTime;
    int price;
    int seat;
    int duration;
    bool valid;

    TicketResult() {
        trainID[0] = '\0';
        from[0] = '\0';
        to[0] = '\0';
        departDay = 0;
        departTime = 0;
        arriveDay = 0;
        arriveTime = 0;
        price = 0;
        seat = 0;
        duration = 0;
        valid = false;
    }
};

string query_ticket(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* from = getParam(keys, values, paramCount, 's');
    const char* to = getParam(keys, values, paramCount, 't');
    const char* date = getParam(keys, values, paramCount, 'd');
    const char* priority = getParam(keys, values, paramCount, 'p');

    int day = dateToDay(date);
    bool sortByTime = (priority == nullptr || strcmp(priority, "time") == 0);

    TicketResult results[MAX_TRAINS];
    int resultCount = 0;

    for (int i = 0; i < trainCount; i++) {
        if (!trains[i].exists || !trains[i].released) continue;

        int startIdx = findStation(i, from);
        int endIdx = findStation(i, to);

        if (startIdx == -1 || endIdx == -1 || startIdx >= endIdx) continue;

        // Calculate the day when the train departs from the starting station
        // We need to find which startStationDay results in departDay == day
        int foundStartDay = -1;
        for (int startStationDay = trains[i].saleDateStart; startStationDay <= trains[i].saleDateEnd; startStationDay++) {
            int arriveDay, arriveTime, departDay, departTime;
            calculateTimes(i, startStationDay, startIdx, arriveDay, arriveTime, departDay, departTime);
            if (departDay == day) {
                foundStartDay = startStationDay;
                break;
            }
        }

        if (foundStartDay == -1) continue;

        // Calculate times using the found start day
        int arriveDay, arriveTime, departDay, departTime;
        calculateTimes(i, foundStartDay, startIdx, arriveDay, arriveTime, departDay, departTime);

        // Verify that departure day matches query date
        if (departDay != day) continue;

        // Calculate arrival time at destination
        calculateTimes(i, foundStartDay, endIdx, arriveDay, arriveTime, departDay, departTime);

        results[resultCount].valid = true;
        strcpy(results[resultCount].trainID, trains[i].trainID);
        strcpy(results[resultCount].from, from);
        strcpy(results[resultCount].to, to);

        // Recalculate departure time from boarding station
        calculateTimes(i, foundStartDay, startIdx, arriveDay, arriveTime, departDay, departTime);
        results[resultCount].departDay = departDay;
        results[resultCount].departTime = departTime;

        // Calculate arrival time at destination
        calculateTimes(i, foundStartDay, endIdx, arriveDay, arriveTime, departDay, departTime);
        results[resultCount].arriveDay = arriveDay;
        results[resultCount].arriveTime = arriveTime;

        results[resultCount].price = calculatePrice(i, startIdx, endIdx);
        results[resultCount].seat = seatManager.querySeat(i, day, startIdx, endIdx, trains[i].stationNum);
        results[resultCount].duration = (arriveDay - day) * 1440 + arriveTime - departTime;
        resultCount++;
    }

    // Sort results
    for (int i = 0; i < resultCount; i++) {
        for (int j = i + 1; j < resultCount; j++) {
            bool swap = false;
            if (sortByTime) {
                if (results[j].duration < results[i].duration) swap = true;
                else if (results[j].duration == results[i].duration && strcmp(results[j].trainID, results[i].trainID) < 0) swap = true;
            } else {
                if (results[j].price < results[i].price) swap = true;
                else if (results[j].price == results[i].price && strcmp(results[j].trainID, results[i].trainID) < 0) swap = true;
            }
            if (swap) {
                TicketResult temp = results[i];
                results[i] = results[j];
                results[j] = temp;
            }
        }
    }

    string result = to_string(resultCount) + "\n";
    for (int i = 0; i < resultCount; i++) {
        char line[200];
        sprintf(line, "%s %s %s %s -> %s %s %s %d %d",
                results[i].trainID,
                results[i].from,
                dayToDate(results[i].departDay).c_str(),
                minutesToTime(results[i].departTime).c_str(),
                results[i].to,
                dayToDate(results[i].arriveDay).c_str(),
                minutesToTime(results[i].arriveTime).c_str(),
                results[i].price,
                results[i].seat);
        result += string(line) + "\n";
    }

    return result;
}

// Process pending orders for a train
void processPendingOrders(int trainIdx, int day) {
    for (int i = 0; i < pendingOrderCount; i++) {
        if (!pendingOrders[i].exists || pendingOrders[i].status != 'p') continue;

        int orderTrainIdx = findTrain(pendingOrders[i].trainID);
        if (orderTrainIdx != trainIdx || pendingOrders[i].day != day) continue;

        int startIdx = pendingOrders[i].startStation;
        int endIdx = pendingOrders[i].endStation;
        int num = pendingOrders[i].num;

        if (seatManager.buyTickets(trainIdx, day, startIdx, endIdx, num, trains[trainIdx].stationNum)) {
            pendingOrders[i].status = 's';
            // Add to regular orders
            for (int j = 0; j < orderCount; j++) {
                if (!orders[j].exists) {
                    orders[j] = pendingOrders[i];
                    orders[j].timestamp = pendingOrders[i].timestamp;
                    break;
                }
            }
        }
    }
}

string buy_ticket(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* username = getParam(keys, values, paramCount, 'u');
    const char* trainID = getParam(keys, values, paramCount, 'i');
    const char* date = getParam(keys, values, paramCount, 'd');
    const char* num_str = getParam(keys, values, paramCount, 'n');
    const char* from = getParam(keys, values, paramCount, 'f');
    const char* to = getParam(keys, values, paramCount, 't');
    const char* queue = getParam(keys, values, paramCount, 'q');

    int user_idx = findUser(username);
    if (user_idx == -1) return "-1";
    if (!users[user_idx].online) return "-1";

    int train_idx = findTrain(trainID);
    if (train_idx == -1) return "-1";
    if (!trains[train_idx].released) return "-1";

    int day = dateToDay(date);
    int num = atoi(num_str);

    if (num <= 0 || num > trains[train_idx].seatNum) return "-1";

    int startIdx = findStation(train_idx, from);
    int endIdx = findStation(train_idx, to);

    if (startIdx == -1 || endIdx == -1 || startIdx >= endIdx) return "-1";

    int price = calculatePrice(train_idx, startIdx, endIdx);

    // Try to buy tickets
    if (seatManager.buyTickets(train_idx, day, startIdx, endIdx, num, trains[train_idx].stationNum)) {
        // Success
        orders[orderCount].exists = true;
        strcpy(orders[orderCount].username, username);
        strcpy(orders[orderCount].trainID, trainID);
        orders[orderCount].day = day;
        orders[orderCount].startStation = startIdx;
        orders[orderCount].endStation = endIdx;
        orders[orderCount].num = num;
        orders[orderCount].price = price;
        orders[orderCount].timestamp = currentTimestamp++;
        orders[orderCount].status = 's';
        orderCount++;

        return to_string(price * num);
    } else {
        // Check if queue is enabled
        if (queue != nullptr && strcmp(queue, "true") == 0) {
            // Add to pending queue
            pendingOrders[pendingOrderCount].exists = true;
            strcpy(pendingOrders[pendingOrderCount].username, username);
            strcpy(pendingOrders[pendingOrderCount].trainID, trainID);
            pendingOrders[pendingOrderCount].day = day;
            pendingOrders[pendingOrderCount].startStation = startIdx;
            pendingOrders[pendingOrderCount].endStation = endIdx;
            pendingOrders[pendingOrderCount].num = num;
            pendingOrders[pendingOrderCount].price = price;
            pendingOrders[pendingOrderCount].timestamp = currentTimestamp++;
            pendingOrders[pendingOrderCount].status = 'p';
            pendingOrderCount++;

            return "queue";
        } else {
            return "-1";
        }
    }
}

string query_order(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* username = getParam(keys, values, paramCount, 'u');

    int user_idx = findUser(username);
    if (user_idx == -1) return "-1";
    if (!users[user_idx].online) return "-1";

    // Collect all orders for this user
    int userOrders[MAX_ORDERS];
    int userOrderCount = 0;

    for (int i = 0; i < orderCount; i++) {
        if (orders[i].exists && strcmp(orders[i].username, username) == 0) {
            userOrders[userOrderCount++] = i;
        }
    }

    for (int i = 0; i < pendingOrderCount; i++) {
        if (pendingOrders[i].exists && pendingOrders[i].status == 'p' &&
            strcmp(pendingOrders[i].username, username) == 0) {
            userOrders[userOrderCount++] = -i - 1;  // Negative index for pending orders
        }
    }

    // Sort by timestamp (newest first)
    for (int i = 0; i < userOrderCount; i++) {
        for (int j = i + 1; j < userOrderCount; j++) {
            int timestamp_i = (userOrders[i] >= 0) ? orders[userOrders[i]].timestamp : pendingOrders[-userOrders[i] - 1].timestamp;
            int timestamp_j = (userOrders[j] >= 0) ? orders[userOrders[j]].timestamp : pendingOrders[-userOrders[j] - 1].timestamp;
            if (timestamp_j > timestamp_i) {
                int temp = userOrders[i];
                userOrders[i] = userOrders[j];
                userOrders[j] = temp;
            }
        }
    }

    string result = to_string(userOrderCount) + "\n";
    for (int i = 0; i < userOrderCount; i++) {
        Order* order;
        if (userOrders[i] >= 0) {
            order = &orders[userOrders[i]];
        } else {
            order = &pendingOrders[-userOrders[i] - 1];
        }

        char statusStr[20];
        if (order->status == 's') strcpy(statusStr, "success");
        else if (order->status == 'p') strcpy(statusStr, "pending");
        else strcpy(statusStr, "refunded");

        int train_idx = findTrain(order->trainID);
        char from[31], to[31];
        strcpy(from, trains[train_idx].stations[order->startStation]);
        strcpy(to, trains[train_idx].stations[order->endStation]);

        int day = order->day;
        int arriveDay, arriveTime, departDay, departTime;
        calculateTimes(train_idx, day, order->startStation, arriveDay, arriveTime, departDay, departTime);
        calculateTimes(train_idx, day, order->endStation, arriveDay, arriveTime, departDay, departTime);

        char line[200];
        sprintf(line, "[%s] %s %s %s %s -> %s %s %s %d %d",
                statusStr,
                order->trainID,
                from,
                dayToDate(departDay).c_str(),
                minutesToTime(departTime).c_str(),
                to,
                dayToDate(arriveDay).c_str(),
                minutesToTime(arriveTime).c_str(),
                order->price,
                order->num);
        result += string(line) + "\n";
    }

    return result;
}

int refund_ticket(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* username = getParam(keys, values, paramCount, 'u');
    const char* num_str = getParam(keys, values, paramCount, 'n');

    int user_idx = findUser(username);
    if (user_idx == -1) return -1;
    if (!users[user_idx].online) return -1;

    int n = (num_str == nullptr) ? 1 : atoi(num_str);

    // Collect all orders for this user
    int userOrders[MAX_ORDERS];
    int userOrderCount = 0;

    for (int i = 0; i < orderCount; i++) {
        if (orders[i].exists && strcmp(orders[i].username, username) == 0) {
            userOrders[userOrderCount++] = i;
        }
    }

    // Sort by timestamp (newest first)
    for (int i = 0; i < userOrderCount; i++) {
        for (int j = i + 1; j < userOrderCount; j++) {
            if (orders[userOrders[j]].timestamp > orders[userOrders[i]].timestamp) {
                int temp = userOrders[i];
                userOrders[i] = userOrders[j];
                userOrders[j] = temp;
            }
        }
    }

    if (n > userOrderCount || n <= 0) return -1;

    int order_idx = userOrders[n - 1];
    if (orders[order_idx].status != 's') return -1;

    // Refund tickets
    int train_idx = findTrain(orders[order_idx].trainID);
    seatManager.refundTickets(train_idx, orders[order_idx].day,
                               orders[order_idx].startStation, orders[order_idx].endStation,
                               orders[order_idx].num, trains[train_idx].stationNum);

    orders[order_idx].status = 'r';

    // Process pending orders for this train and day
    processPendingOrders(train_idx, orders[order_idx].day);

    return 0;
}

int clean() {
    // Clear all data
    for (int i = 0; i < userCount; i++) {
        users[i].exists = false;
        users[i].online = false;
    }
    userCount = 0;

    for (int i = 0; i < trainCount; i++) {
        seatManager.clearTrain(i);
        trains[i].exists = false;
        trains[i].released = false;
    }
    trainCount = 0;

    for (int i = 0; i < orderCount; i++) {
        orders[i].exists = false;
    }
    orderCount = 0;

    for (int i = 0; i < pendingOrderCount; i++) {
        pendingOrders[i].exists = false;
    }
    pendingOrderCount = 0;

    currentTimestamp = 0;

    return 0;
}

string exit() {
    // Log out all users
    for (int i = 0; i < userCount; i++) {
        users[i].online = false;
    }
    return "bye";
}

// Query transfer
string query_transfer(const string& params) {
    char keys[10][2], values[10][1000];
    int paramCount;
    parseParams(params, keys, values, paramCount);

    const char* from = getParam(keys, values, paramCount, 's');
    const char* to = getParam(keys, values, paramCount, 't');
    const char* date = getParam(keys, values, paramCount, 'd');
    const char* priority = getParam(keys, values, paramCount, 'p');

    int day = dateToDay(date);
    bool sortByTime = (priority == nullptr || strcmp(priority, "time") == 0);

    // First, check if direct route exists
    bool hasDirect = false;
    for (int i = 0; i < trainCount; i++) {
        if (!trains[i].exists || !trains[i].released) continue;
        int startIdx = findStation(i, from);
        int endIdx = findStation(i, to);
        if (startIdx != -1 && endIdx != -1 && startIdx < endIdx) {
            hasDirect = true;
            break;
        }
    }

    // If direct route exists, return 0
    if (hasDirect) {
        return "0";
    }

    // Find optimal transfer
    TicketResult bestResult1, bestResult2;
    bool found = false;
    int bestValue = sortByTime ? 0x7FFFFFFF : 0x7FFFFFFF;  // Use int for time, large for cost

    for (int i = 0; i < trainCount; i++) {
        if (!trains[i].exists || !trains[i].released) continue;

        int startIdx1 = findStation(i, from);
        if (startIdx1 == -1) continue;

        // Check if train i has from station
        // Find all stations that train i goes to
        for (int stationIdx = startIdx1 + 1; stationIdx < trains[i].stationNum; stationIdx++) {
            char* transferStation = trains[i].stations[stationIdx];

            // Find trains that go from transferStation to to
            for (int j = 0; j < trainCount; j++) {
                if (!trains[j].exists || !trains[j].released) continue;
                if (i == j) continue;  // Cannot transfer to same train

                int startIdx2 = findStation(j, transferStation);
                int endIdx2 = findStation(j, to);

                if (startIdx2 == -1 || endIdx2 == -1 || startIdx2 >= endIdx2) continue;

                // Check if within sale date range for train 1
                int train1Day = day;
                for (int k = 0; k < startIdx1; k++) {
                    train1Day -= (trains[i].travelTimes[k] + (k < trains[i].stationNum - 2 ? trains[i].stopoverTimes[k] : 0)) / 1440;
                }
                if (train1Day < trains[i].saleDateStart || train1Day > trains[i].saleDateEnd) continue;

                // Calculate departure time for train 1
                int arriveDay1, arriveTime1, departDay1, departTime1;
                calculateTimes(i, day, startIdx1, arriveDay1, arriveTime1, departDay1, departTime1);

                if (departDay1 != day) continue;  // Departure day must match query date

                // Calculate arrival time at transfer station
                int transferArriveDay, transferArriveTime, transferDepartDay, transferDepartTime;
                calculateTimes(i, day, stationIdx, transferArriveDay, transferArriveTime, transferDepartDay, transferDepartTime);

                // Check if within sale date range for train 2
                int train2BaseDay = transferArriveDay;
                int train2StartDay = train2BaseDay;
                for (int k = 0; k < startIdx2; k++) {
                    train2StartDay -= (trains[j].travelTimes[k] + (k < trains[j].stationNum - 2 ? trains[j].stopoverTimes[k] : 0)) / 1440;
                }
                if (train2StartDay < trains[j].saleDateStart || train2StartDay > trains[j].saleDateEnd) continue;

                // Calculate departure and arrival times for train 2
                int arriveDay2, arriveTime2, departDay2, departTime2;
                calculateTimes(j, train2BaseDay, startIdx2, arriveDay2, arriveTime2, departDay2, departTime2);
                calculateTimes(j, train2BaseDay, endIdx2, arriveDay2, arriveTime2, departDay2, departTime2);

                // Calculate total time and price
                int totalTime = (arriveDay2 - day) * 1440 + arriveTime2 - departTime1;
                int totalPrice = calculatePrice(i, startIdx1, stationIdx) + calculatePrice(j, startIdx2, endIdx2);

                int currentValue = sortByTime ? totalTime : totalPrice;

                // Check if this is better
                bool isBetter = false;
                if (!found) {
                    isBetter = true;
                } else if (currentValue < bestValue) {
                    isBetter = true;
                } else if (currentValue == bestValue) {
                    // Tie-breaking: if time, try to minimize riding time on train 1
                    if (sortByTime) {
                        int train1Time = (transferArriveDay - day) * 1440 + transferArriveTime - departTime1;
                        int bestTrain1Time = (bestResult1.arriveDay - day) * 1440 + bestResult1.arriveTime - bestResult1.departTime;
                        if (train1Time < bestTrain1Time) {
                            isBetter = true;
                        }
                    }
                }

                if (isBetter) {
                    found = true;
                    bestValue = currentValue;

                    // Store result for train 1
                    strcpy(bestResult1.trainID, trains[i].trainID);
                    strcpy(bestResult1.from, from);
                    strcpy(bestResult1.to, transferStation);
                    bestResult1.departDay = departDay1;
                    bestResult1.departTime = departTime1;
                    bestResult1.arriveDay = transferArriveDay;
                    bestResult1.arriveTime = transferArriveTime;
                    bestResult1.price = calculatePrice(i, startIdx1, stationIdx);
                    bestResult1.seat = seatManager.querySeat(i, day, startIdx1, stationIdx, trains[i].stationNum);
                    bestResult1.duration = (transferArriveDay - day) * 1440 + transferArriveTime - departTime1;
                    bestResult1.valid = true;

                    // Store result for train 2
                    strcpy(bestResult2.trainID, trains[j].trainID);
                    strcpy(bestResult2.from, transferStation);
                    strcpy(bestResult2.to, to);
                    bestResult2.departDay = departDay2;
                    bestResult2.departTime = departTime2;
                    bestResult2.arriveDay = arriveDay2;
                    bestResult2.arriveTime = arriveTime2;
                    bestResult2.price = calculatePrice(j, startIdx2, endIdx2);
                    bestResult2.seat = seatManager.querySeat(j, train2BaseDay, startIdx2, endIdx2, trains[j].stationNum);
                    bestResult2.duration = (arriveDay2 - train2BaseDay) * 1440 + arriveTime2 - departTime2;
                    bestResult2.valid = true;
                }
            }
        }
    }

    if (!found) {
        return "0";
    }

    // Return the two trains
    string result;
    char line1[200];
    sprintf(line1, "%s %s %s %s -> %s %s %s %d %d",
            bestResult1.trainID,
            bestResult1.from,
            dayToDate(bestResult1.departDay).c_str(),
            minutesToTime(bestResult1.departTime).c_str(),
            bestResult1.to,
            dayToDate(bestResult1.arriveDay).c_str(),
            minutesToTime(bestResult1.arriveTime).c_str(),
            bestResult1.price,
            bestResult1.seat);
    result += string(line1) + "\n";

    char line2[200];
    sprintf(line2, "%s %s %s %s -> %s %s %s %d %d",
            bestResult2.trainID,
            bestResult2.from,
            dayToDate(bestResult2.departDay).c_str(),
            minutesToTime(bestResult2.departTime).c_str(),
            bestResult2.to,
            dayToDate(bestResult2.arriveDay).c_str(),
            minutesToTime(bestResult2.arriveTime).c_str(),
            bestResult2.price,
            bestResult2.seat);
    result += string(line2) + "\n";

    return result;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;

        int spacePos = line.find(' ');
        if (spacePos == -1) spacePos = line.length();

        string cmd = line.substr(0, spacePos);
        string params = (spacePos < line.length()) ? line.substr(spacePos + 1) : "";

        if (cmd == "add_user") {
            cout << add_user(params) << endl;
        } else if (cmd == "login") {
            cout << login(params) << endl;
        } else if (cmd == "logout") {
            cout << logout(params) << endl;
        } else if (cmd == "query_profile") {
            cout << query_profile(params) << endl;
        } else if (cmd == "modify_profile") {
            cout << modify_profile(params) << endl;
        } else if (cmd == "add_train") {
            cout << add_train(params) << endl;
        } else if (cmd == "release_train") {
            cout << release_train(params) << endl;
        } else if (cmd == "query_train") {
            cout << query_train(params);
        } else if (cmd == "delete_train") {
            cout << delete_train(params) << endl;
        } else if (cmd == "query_ticket") {
            cout << query_ticket(params);
        } else if (cmd == "buy_ticket") {
            cout << buy_ticket(params) << endl;
        } else if (cmd == "query_order") {
            cout << query_order(params);
        } else if (cmd == "refund_ticket") {
            cout << refund_ticket(params) << endl;
        } else if (cmd == "clean") {
            cout << clean() << endl;
        } else if (cmd == "exit") {
            cout << exit() << endl;
            break;
        } else if (cmd == "query_transfer") {
            cout << query_transfer(params);
        }
    }

    return 0;
}
