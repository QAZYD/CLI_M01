#pragma once
class IETThread
{
    public:
        IETThread() = default;
       virtual ~IETThread() = default;

        void start();
        static void sleep(int ms);

    protected:
        virtual void run() = 0;

};