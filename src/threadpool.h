

#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <list>
#include <iostream>
#include <functional>
#include <condition_variable>
#include <future>




class ThreadPool {
    using task = std::function<void()>;

public:
   
    unsigned ThreadCount;

    ThreadPool(unsigned ThreadCount_)
    : threads(ThreadCount_) 
    , jobs_left( 0 )
    , stop( false )
    , ThreadCount(ThreadCount_)
{   
    for( unsigned i = 0; i < ThreadCount; ++i )
        threads[ i ] = std::move( std::thread( [this,i]{ 
        while( !stop ) {
        task newjob = next(i);
        newjob();
        --jobs_left;
        wait_var.notify_one();
    } } ) );
}




task next(int i) {
    task res;
    std::unique_lock<std::mutex> job_lock( queue_mutex );

    // Wait for a job if we don't have any.
    job_needed.wait( job_lock, [this]() ->bool { return queue.size() || stop; } );
    
    
    if( !stop ) {
        res = queue.front();
        queue.pop_front();
        std::thread::id this_id = std::this_thread::get_id();
        std::cout << "Got Job! Thread ID " << i << std::endl;
    }
    else { 
        res = []{};
        ++jobs_left;
    }
    return res;
}

    
    template<class F, class... Args>
    auto Add(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {

        using return_type = typename std::result_of<F(Args...)>::type;

        auto task1 = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> res = task1->get_future();
        {
        std::lock_guard<std::mutex> lock( queue_mutex );
        queue.emplace_back([task1](){ (*task1)(); });
        ++jobs_left;
    
        }

        job_needed.notify_one();
        return res;
    }




     ~ThreadPool() {
        if( !stop ) {
                
             if( jobs_left > 0 ) {
            std::unique_lock<std::mutex> lk( wait_mutex );
            wait_var.wait( lk, [this]{ return this->jobs_left == 0; } );
            lk.unlock();
                }

        
            stop = true;
            job_needed.notify_all();

            for( auto &x : threads )
                    x.join();
           
        }
    }

private:
    

    std::vector<std::thread> threads;
    std::list<task> queue;
    std::atomic_int         jobs_left;
    std::atomic_bool        stop;
    std::condition_variable job_needed;
    std::condition_variable wait_var;
    std::mutex              wait_mutex;
    std::mutex              queue_mutex;

};


/*
int main()
{
    
    ThreadPool pool(4);
    std::vector< std::future<int> > results;

    for(int i = 0; i < 8; ++i) {
        results.emplace_back(
            pool.Add([i] {
                std::cout << "hello " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "world " << i << std::endl;
                return i*i;
            })
        );
    }

    for(auto && result: results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;
    
    return 0;
}
*/