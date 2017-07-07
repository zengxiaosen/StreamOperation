// priority_queue::push/pop
#include <iostream>       // std::cout
#include <queue>          // std::priority_queue

struct RtpPacket {
  int priority;
  unsigned int seqn;
  int data;
  RtpPacket(int priority_, int seqn_, int data_) {
    priority = priority_;
    seqn = seqn_;
    data = data_;
  };
};

struct SenderPacket {
  int priority;
  unsigned char* buf;
  int buf_size;
};

class Compare
{
public:
    bool operator() (RtpPacket a, RtpPacket b)
    {
      if (a.priority > b.priority) {
        return true;
      } else if (a.priority == b.priority) {
        return a.seqn < b.seqn;
      } else {
        return false;
      }
    }
};

/*
class PacedSender {
public:
  Insert();
private:
  thread* sender_thread;
  std::priority_queue<SenderPacket, std::vector<SenderPacket>, Compare> pq_;
};

void PacedSender::sender_thread_func() {
  while (running_) {
    while (!pq_.empty()) {

    }
  }
}
*/

int main ()
{
  std::priority_queue<RtpPacket, std::vector<RtpPacket>, Compare> mypq;

  mypq.push(RtpPacket(1, 1, 123));
  mypq.push(RtpPacket(10, 2, 1234));
  mypq.push(RtpPacket(1, 3, 12345));
  mypq.push(RtpPacket(10, 4, 123456));

  std::cout << "Popping out elements...";
  while (!mypq.empty())
    {
      RtpPacket p = mypq.top();
      std::cout << " pr:" << p.priority << "    ";
      std::cout << " seqn:" << p.seqn << "   ";
      std::cout << " data:" << p.data << std::endl;
      mypq.pop();
    }
  std::cout << '\n';

  return 0;
}
