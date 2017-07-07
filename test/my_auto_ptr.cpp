template<class T>
class my_auto_ptr{
private:
    T* m_ptr;
    T* GetPtr(){
        //供构造赋值时使用
        T* tmp = m_ptr;
        m_ptr = 0;
        return tmp;
    }

public:
    explicit my_auto_ptr(T* p=0):m_ptr(p){}
    ~my_auto_ptr(){
        delete m_ptr;
    }
    T& operator*(){
        return *m_ptr;
    }
    T* operator->(){
        return m_ptr;
    };
    my_auto_ptr(my_auto_ptr& mp){
        //复制构造函数
        m_ptr = mp.GetPtr();//mp复制过来后它自己原来的指针相当于失效了

    }

    my_auto_ptr& operator=(my_auto_ptr& ap){
        //造型赋值操作符
        if(ap != *this){
            delete m_ptr;
            m_ptr = ap.GetPtr();
        }
        return *this;
    }

    void reset(T* p){
        //指针重置，相当于把指针指向另外一个地方
        if(p != m_ptr){
            delete m_ptr;
            m_ptr = p;
        }
    }


};

struct Arwen{
    int age;
    Arwen(int gg):age(gg){

    }
};

void main(){
    my_auto_ptr<Arwen> myPtr(new Arwen(24));
    int num = myPtr->age;

    my_auto_ptr<Arwen> ptrOne(myPtr);
    num = ptrOne->age;

    my_auto_ptr<Arwen> ptrTwo = ptrOne;
    num = ptrTwo->age;

    Arwen* pArwen = new Arwen(88);
    ptrTwo.reset(pArwen);
    num = pArwen->age;
    return 0;
}