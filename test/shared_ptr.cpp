//
// Created by yiqixie on 7/4/17.
//

#include <iostream>
using namespace std;
template <class T>
class shared_ptr{
private:
    T* m_ptr;
    //引用计数，表示有多少个智能指针对象拥有m_ptr指向的内存块
    unsigned int shared_count;
public:
    shared_ptr(T* p):m_ptr(p),shared_count(1){}
    ~shared_ptr(){
        deconstruct();
    }
    void deconstruct(){
        if(shared_count == 1){
            //引用计数为1表示只有一个对象使用指针指向的内存快
            delete m_ptr;
            m_ptr = 0;
        }
        shared_count--;
    }
    T& operator*(){
        return *m_ptr;
    }
    T* operator->(){
        return m_ptr;
    }
    //复制构造函数
    shared_ptr(shared_ptr& sp):m_ptr(sp.m_ptr),shared_count(sp.shared_count){
        shared_count++;
    }
    //重载运算符=
    shared_ptr& operator=(shared_ptr& sp){
        sp.shared_count++;
        deconstruct();//相当于先删除左值，然后再通过右值赋值
        m_ptr = sp.m_ptr;
        shared_count = sp.shared_count;
        return *this;
    }
};

struct Arwen{
    int age;
    Arwen(int gg):age(gg){};
};


void main(){
    shared_ptr<Arwen> myPtr(new Arwen(24));
    int num = myPtr->age;
    shared_ptr<Arwen> ptrOne(myPtr);
    num = myPtr->age;
    num = ptrOne->age;
    shared_ptr<Arwen> ptrTwo = ptrOne;
    num = ptrOne->age;
    num = ptrTwo->age;
    return 0;
}