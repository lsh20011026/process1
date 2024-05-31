
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <algorithm>

std::mutex mtx;
std::atomic<bool> terminateFlag(false);

template<typename T>
class Queue {
public:
    struct Node {
        T data;
        Node* next;
        std::chrono::system_clock::time_point wakeUpTime;
        Node(T val, std::chrono::system_clock::time_point time = std::chrono::system_clock::now()) : data(val), next(nullptr), wakeUpTime(time) {}
    };

    Node* front;
    Node* rear;
    int size;

public:
    Queue() : front(nullptr), rear(nullptr), size(0) {}

    ~Queue() {
        while (!isEmpty()) {
            dequeue();
        }
    }

    void enqueue(T value, std::chrono::system_clock::time_point wakeUpTime = std::chrono::system_clock::now()) {
        Node* newNode = new Node(value, wakeUpTime);
        if (isEmpty()) {
            front = rear = newNode;
        }
        else {
            rear->next = newNode;
            rear = newNode;
        }
        size++;
    }

    void dequeue() {
        if (isEmpty()) {
            return;
        }
        Node* temp = front;
        front = front->next;
        delete temp;
        size--;
        if (front == nullptr) {
            rear = nullptr;
        }
    }

    T getFront() const {
        if (isEmpty()) {
            exit(1);
        }
        return front->data;
    }

    bool isEmpty() const {
        return front == nullptr;
    }

    int getSize() const {
        return size;
    }

    void enqueueAtEnd(T value, std::chrono::system_clock::time_point wakeUpTime = std::chrono::system_clock::now()) {
        enqueue(value, wakeUpTime);
    }

    struct CompareByTime {
        bool operator()(const Node* a, const Node* b) const {
            return a->wakeUpTime < b->wakeUpTime;
        }
    };
};

template<typename T>
class Stack {
public:
    Queue<T> q1;
    bool isBackground;
    typename Queue<T>::Node* P;

public:
    Stack(bool isBackgroundStack = false) : isBackground(isBackgroundStack), P(nullptr) {}

    void push(T value) {
        if (isBackground) {
            q1.enqueue(value);
        }
        else {
            int n = q1.getSize();
            for (int i = 0; i < n; ++i) {
                T front = q1.getFront();
                q1.dequeue();
                q1.enqueueAtEnd(front);
            }
            q1.enqueue(value);
        }
    }

    void pop() {
        if (q1.isEmpty()) {
            return;
        }
        q1.dequeue();
        if (q1.isEmpty() && isBackground) {
            P = nullptr;
        }
    }

    T top() {
        if (q1.isEmpty()) {
            exit(1);
        }
        return q1.getFront();
    }

    bool isEmpty() const {
        return q1.isEmpty();
    }

    int getSize() const {
        return q1.getSize();
    }

    void removeProcess(const T process) {
        Queue<T> tempQueue;
        while (!q1.isEmpty()) {
            T front = q1.getFront();
            q1.dequeue();
            if (front != process) {
                tempQueue.enqueue(front);
            }
        }
        while (!tempQueue.isEmpty()) {
            T front = tempQueue.getFront();
            tempQueue.dequeue();
            q1.enqueue(front);
        }
    }

    void promote(Stack<T>& upperStack) {
        if (P == nullptr && !isEmpty()) {
            P = q1.front;
        }
        if (P != nullptr) {
            upperStack.push(P->data);
            removeProcess(P->data);
            if (P->next != nullptr) {
                P = P->next;
            }
            else {
                P = nullptr; // 마지막 노드까지 이동한 경우 P를 nullptr로 설정
            }
        }
        if (isEmpty()) {
            P = nullptr;
        }
    }

    void split_n_merge(Stack<T>& upperStack, int threshold) {
        if (getSize() > threshold) {
            int halfSize = getSize() / 2;
            Queue<T> tempQueue;
            for (int i = 0; i < halfSize; ++i) {
                tempQueue.enqueue(q1.getFront());
                q1.dequeue();
            }
            while (!tempQueue.isEmpty()) {
                upperStack.push(tempQueue.getFront());
                tempQueue.dequeue();
            }
            upperStack.split_n_merge(upperStack, threshold);

            // 재귀 호출 후 P 포인터 초기화
            if (!isEmpty()) {
                P = q1.front;
            }
            else {
                P = nullptr;
            }
        }
        else {
            P = nullptr; // 재귀 호출되지 않은 경우에도 P를 nullptr로 설정
        }
    }

    // Print the structure of the stack and queue
    void printStackAndQueueStructure() const {
        std::cout << "DQ:        [";
        std::vector<T> stackContents;
        typename Queue<T>::Node* tempNode = q1.front;
        while (tempNode != nullptr) {
            stackContents.push_back(tempNode->data);
            tempNode = tempNode->next;
        }

        for (auto it = stackContents.rbegin(); it != stackContents.rend(); ++it) {
            std::cout << *it;
            if (it != stackContents.rend() - 1) {
                std::cout << "F, ";
            }
            else {
                std::cout << "F";
            }
        }

        if (P != nullptr) {
            std::cout << " *";
        }

        std::cout << "] (bottom)" << std::endl;
    }

};

void processAndPrint(Stack<int>& foregroundStack, Stack<int>& backgroundStack, int X, int Y) {
    int pid = 0;
    std::vector<Queue<int>::Node*> waitingProcesses;

    while (!terminateFlag) {
        std::this_thread::sleep_for(std::chrono::seconds(X));

        {
            std::lock_guard<std::mutex> guard(mtx);

            std::cout << "Running: [";
            if (!backgroundStack.isEmpty()) {
                std::cout << backgroundStack.top() << "B ";
            }
            if (!foregroundStack.isEmpty()) {
                std::cout << foregroundStack.top() << "F*";
            }
            std::cout << "]" << std::endl;
            std::cout << "---------------------------" << std::endl;

            foregroundStack.printStackAndQueueStructure();

            std::cout << "---------------------------" << std::endl;
            std::cout << "WQ: [";
            // Waiting Queue 출력
            std::sort(waitingProcesses.begin(), waitingProcesses.end(), typename Queue<int>::CompareByTime());
            for (size_t i = 0; i < waitingProcesses.size(); ++i) {
                auto remainingTime = std::chrono::duration_cast<std::chrono::seconds>(waitingProcesses[i]->wakeUpTime - std::chrono::system_clock::now()).count();
                std::cout << waitingProcesses[i]->data << "(" << remainingTime << ")";
                if (i != waitingProcesses.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]" << std::endl;
            std::cout << "---------------------------" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(Y));
        auto currentTime = std::chrono::system_clock::now();
        for (auto it = waitingProcesses.begin(); it != waitingProcesses.end();) {
            auto remainingTime = std::chrono::duration_cast<std::chrono::seconds>((*it)->wakeUpTime - currentTime).count();
            if (remainingTime <= 0) {
                foregroundStack.push((*it)->data);
                it = waitingProcesses.erase(it);
            }
            else {
                ++it;
            }
        }

        foregroundStack.promote(backgroundStack);
        foregroundStack.split_n_merge(backgroundStack, 2);
    }
}

int main() {
    Stack<int> backgroundStack(true);
    Stack<int> foregroundStack;
    backgroundStack.push(1);
    backgroundStack.push(4);
    foregroundStack.push(6);
    foregroundStack.push(5);
    foregroundStack.push(4);
    int X = 1; // 수정된 X 값
    int Y = 2; // 수정된 Y 값

    std::thread processAndPrintThread([&]() {
        processAndPrint(foregroundStack, backgroundStack, X, Y);
        });

    std::cin.get();
    std::cin.get();

    terminateFlag = true;

    processAndPrintThread.join();

    return 0;
}