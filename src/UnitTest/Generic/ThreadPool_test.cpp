/*
 * Author: Fabian Sandoval Saldias (fabianvss@gmail.com)
 * Date: 2018-06-12
 */

#include <ambthread/ThreadPool.h>

#include <UnitTest/Helpers.h>
#include <catch2/catch.hpp>

TEST_CASE("ThreadPool executes tasks", "[ThreadPool]")
{
    using namespace std::chrono_literals;
    auto sleep = []() { std::this_thread::sleep_for(1ms); };

    SECTION("lateInit() works")
    {
        auto latePool = ambthread::ThreadPool{};
        latePool.lateInit(4);
        int x = 4;
        latePool.submit([&]() { x = 5; });
        latePool.wait();
        REQUIRE(x == 5);
    }

    auto pool = ambthread::ThreadPool{4};

    SECTION("Single function without future returns")
    {
        int x = 4;
        pool.submit(
            [&]()
            {
                sleep();
                x = 5;
                return x;
            });
        pool.wait();
        REQUIRE(x == 5);
    }

    SECTION("Single function with future returns")
    {
        int x = 4;
        auto future = pool.submitUsingFuture(
            [&]()
            {
                sleep();
                x = 5;
                return x;
            });
        pool.wait();
        REQUIRE(x == 5);
        REQUIRE(future.get() == 5);
    }

    SECTION("Single function with waiting future returns")
    {
        int x = 4;
        auto future = pool.submitUsingFuture(
            [&]()
            {
                sleep();
                x = 5;
                return x;
            });
        REQUIRE(future.get() == 5);
    }

    SECTION("Single task without future returns (using Task constructor)")
    {
        int x = 4;
        pool.submit(ambthread::ThreadPool::Task{[&]()
                                                {
                                                    sleep();
                                                    x = 5;
                                                    return x;
                                                }});
        pool.wait();
        REQUIRE(x == 5);
    }

    SECTION("Single task without future returns (using setFunction())")
    {
        int x = 4;
        ambthread::ThreadPool::Task t;
        t.setFunction(
            [&]()
            {
                sleep();
                x = 5;
                return x;
            });
        pool.submit(std::move(t));
        pool.wait();
        REQUIRE(x == 5);
    }

    SECTION("Single task with future returns")
    {
        int x = 4;
        ambthread::ThreadPool::Task t;
        auto future = t.setFunctionUsingFuture(
            [&]()
            {
                sleep();
                x = 5;
                return x;
            });
        pool.submit(std::move(t));
        pool.wait();
        REQUIRE(x == 5);
        REQUIRE(future.get() == 5);
    }

    SECTION("Single task with waiting future returns")
    {
        int x = 4;
        ambthread::ThreadPool::Task t;
        auto future = t.setFunctionUsingFuture(
            [&]()
            {
                sleep();
                x = 5;
                return x;
            });
        pool.submit(std::move(t));
        REQUIRE(future.get() == 5);
    }

    SECTION("Task constructs with function")
    {
        int x = 4;
        ambthread::ThreadPool::Task t{[&]() { x = 5; }};
        pool.submit(std::move(t));
        pool.wait();
        REQUIRE(x == 5);
    }

    SECTION("pending(), empty() etc. work")
    {
        /*
         * Dependency tree:
         *   t2
         *    |
         *   t1
         */
        size_t pending1 = 0;
        size_t pending2 = 0;
        bool empty1 = true;
        bool empty2 = true;
        bool idle1 = true;
        bool idle2 = true;
        bool busy1 = false;
        bool busy2 = false;
        ambthread::ThreadPool::Task t1{[&]()
                                       {
                                           pending1 = pool.pending();
                                           empty1 = pool.empty();
                                           idle1 = pool.idle();
                                           busy1 = pool.busy();
                                       }};
        ambthread::ThreadPool::Task t2{[&]()
                                       {
                                           pending2 = pool.pending();
                                           empty2 = pool.empty();
                                           idle2 = pool.idle();
                                           busy2 = pool.busy();
                                       }};

        t1.addDependency(std::move(t2));

        pool.submit(std::move(t1));
        pool.wait();

        REQUIRE(pending1 == 1);
        REQUIRE(pending2 == 2);
        REQUIRE(!empty1);
        REQUIRE(!empty2);
        REQUIRE(!idle1);
        REQUIRE(!idle2);
        REQUIRE(busy1);
        REQUIRE(busy2);
        REQUIRE(pool.pending() == 0);
        REQUIRE(pool.empty());
        REQUIRE(pool.idle());
        REQUIRE(!pool.busy());
    }

    SECTION("Tasks with dependencies run")
    {
        /*
         * Dependency tree:
         *   a  c
         *   |  |
         *   b  d
         *    \/
         *     e  g
         *      \/
         *       f
         */

        int a = 1;
        int b = 2;
        int c = 3;
        int d = 4;
        int e = 5;
        int f = 6;
        int g = 7;
        ambthread::ThreadPool::Task ta{[&]() { a = 8; }};
        ambthread::ThreadPool::Task tb{[&]() { b = 9; }};
        ambthread::ThreadPool::Task tc{[&]() { c = 10; }};
        ambthread::ThreadPool::Task td{[&]() { d = 11; }};
        ambthread::ThreadPool::Task te{[&]() { e = 12; }};
        ambthread::ThreadPool::Task tf{[&]() { f = 13; }};
        ambthread::ThreadPool::Task tg{[&]() { g = 14; }};

        tb.addDependency(std::move(ta));

        td.addDependency(std::move(tc));

        te.addDependency(std::move(tb));
        te.addDependency(std::move(td));

        tf.addDependency(std::move(te));
        tf.addDependency(std::move(tg));

        pool.submit(std::move(tf));
        pool.wait();

        REQUIRE(a == 8);
        REQUIRE(b == 9);
        REQUIRE(c == 10);
        REQUIRE(d == 11);
        REQUIRE(e == 12);
        REQUIRE(f == 13);
        REQUIRE(g == 14);
    }

    SECTION("Tasks with dependencies run respecting order")
    {
        /*
         * Dependency tree:
         *   a  c     <-- add 1 to b/d (a sleeps before)
         *   |  |
         *   b  d     <-- add b/d to b/d (d sleeps before)
         *    \/
         *     e      <-- add b and d
         *      \  g  <-- add 1 to f
         *       \/
         *        f   <-- add e
         */

        int b = 0;
        int d = 0;
        int e = 0;
        int f = 0;
        ambthread::ThreadPool::Task ta{[&]()
                                       {
                                           sleep();
                                           b++;
                                       }};
        ambthread::ThreadPool::Task tb{[&]() { b += b; }};
        ambthread::ThreadPool::Task tc{[&]() { d++; }};
        ambthread::ThreadPool::Task td{[&]()
                                       {
                                           sleep();
                                           d += d;
                                       }};
        ambthread::ThreadPool::Task te{[&]() { e += b + d; }};
        ambthread::ThreadPool::Task tf{[&]() { f += e; }};
        ambthread::ThreadPool::Task tg{[&]() { f += 1; }};

        tb.addDependency(std::move(ta));

        td.addDependency(std::move(tc));

        te.addDependency(std::move(tb));
        te.addDependency(std::move(td));

        tf.addDependency(std::move(te));
        tf.addDependency(std::move(tg));

        pool.submit(std::move(tf));
        pool.wait();

        REQUIRE(b == 2);
        REQUIRE(d == 2);
        REQUIRE(e == 4);
        REQUIRE(f == 5);
    }

    SECTION("Tasks with dependency graph run respecting order")
    {
        /*
         * Dependency graph:
         *       a
         *      / \
         *     b   e    <-- e sleeps to see d and f waiting
         *    / \ / \
         *   c   d   f
         */

        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        int e = 0;
        int f = 0;
        auto ta = std::make_shared<ambthread::ThreadPool::Task>([&]() { a++; });
        auto tb = std::make_shared<ambthread::ThreadPool::Task>([&]() { b = a + 1; });
        auto tc = std::make_shared<ambthread::ThreadPool::Task>([&]() { c = b + 1; });
        auto td = std::make_shared<ambthread::ThreadPool::Task>([&]() { d = b + e; });
        auto te = std::make_shared<ambthread::ThreadPool::Task>(
            [&]()
            {
                sleep();
                e = a + 1;
            });
        auto tf = std::make_shared<ambthread::ThreadPool::Task>([&]() { f = e + 1; });

        tb->addDependency(ta);

        te->addDependency(ta);

        tc->addDependency(tb);

        td->addDependency(tb);
        td->addDependency(te);

        tf->addDependency(te);

        pool.submit(tc);
        pool.submit(td);
        pool.submit(tf);
        pool.wait();

        REQUIRE(a == 1);
        REQUIRE(b == 2);
        REQUIRE(c == 3);
        REQUIRE(d == 4);
        REQUIRE(e == 2);
        REQUIRE(f == 3);
    }

    SECTION("Task can emit new task")
    {
        int a = 0;
        ambthread::ThreadPool::Task ta{[&]() { pool.submit([&]() { a++; }); }};

        pool.submit(std::move(ta));
        pool.wait();

        REQUIRE(a == 1);
    }

    SECTION("Task can emit new task with dependencies")
    {
        /*
         * Dependency graph:
         *   a
         *   |\
         *   | b  <-- emitted via a
         *   |/
         *   c
         */
        int a = 0;
        int b = 0;
        int c = 0;
        auto ta = std::make_shared<ambthread::ThreadPool::Task>();
        auto tb = std::make_shared<ambthread::ThreadPool::Task>([&]() { b = a + 1; });
        auto tc = std::make_shared<ambthread::ThreadPool::Task>([&]() { c = b + 1; });

        ta->setFunction(
            [&]()
            {
                sleep();
                a++;
                tb->addDependency(ta);
                {
                    auto lock{pool.taskLockGuard()};
                    tc->addDependency(tb);
                }
            });

        tc->addDependency(ta);
        pool.submit(tc);
        pool.wait();

        REQUIRE(a == 1);
        REQUIRE(b == 2);
        REQUIRE(c == 3);
    }

    SECTION("Task can abort dependents")
    {
        /*
         * Dependency tree:
         *         a
         *        /
         *   e   b    <-- b aborts
         *    \ /
         *     c      <-- c should not be called
         *     |
         *     d      <-- d should not be called
         */
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        int e = 0;

        ambthread::ThreadPool::Task ta{[&]() { a++; }};

        auto tb = std::make_shared<ambthread::ThreadPool::Task>();
        tb->setFunction(
            [&]()
            {
                sleep();
                b++;
                tb->setFlow(ambthread::ThreadPool::Task::Flow::stopDependents);
            });

        ambthread::ThreadPool::Task tc{[&]() { c++; }};
        ambthread::ThreadPool::Task td{[&]() { d++; }};
        ambthread::ThreadPool::Task te{[&]() { e++; }};

        tb->addDependency(std::move(ta));
        tc.addDependency(std::move(te));
        tc.addDependency(tb);
        td.addDependency(std::move(tc));
        pool.submit(std::move(td));
        pool.wait();

        REQUIRE(a == 1);
        REQUIRE(b == 1);
        REQUIRE(c == 0);
        REQUIRE(d == 0);
        REQUIRE(e == 1);
    }

    SECTION("Task can abort everything via task flow")
    {
        /*
         * Note: Flow::stopAll cannot easily be tested more extensive,
         * because you cannot guarantie which task already run,
         * you only know that dependents were not run.
         *
         * Dependency tree:
         *   a
         *   |
         *   b  <-- b aborts
         *   |
         *   c  <-- c should not be called
         *   |
         *   d  <-- d should not be called
         */
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;

        ambthread::ThreadPool::Task ta{[&]() { a++; }};

        auto tb = std::make_shared<ambthread::ThreadPool::Task>();
        tb->setFunction(
            [&]()
            {
                sleep();
                b++;
                tb->setFlow(ambthread::ThreadPool::Task::Flow::stopAll);
            });

        ambthread::ThreadPool::Task tc{[&]() { c++; }};
        ambthread::ThreadPool::Task td{[&]() { d++; }};

        tb->addDependency(std::move(ta));
        tc.addDependency(tb);
        td.addDependency(std::move(tc));
        pool.submit(std::move(td));
        pool.wait();

        REQUIRE(a == 1);
        REQUIRE(b == 1);
        REQUIRE(c == 0);
        REQUIRE(d == 0);
        REQUIRE(pool.empty());
        REQUIRE(!pool.stopping());

        int e = 0;
        pool.submit([&]() { e++; });
        pool.wait();
        REQUIRE(e == 1);
        REQUIRE(!pool.stopping());
    }

    SECTION("Task can abort everything via thread pool")
    {
        /*
         * Note: Flow::stopAll cannot easily be tested more extensive,
         * because you cannot guarantie which task already run,
         * you only know that dependents were not run.
         *
         * Dependency tree:
         *   a
         *   |
         *   b  <-- b aborts
         *   |
         *   c  <-- c should not be called
         *   |
         *   d  <-- d should not be called
         */
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;

        ambthread::ThreadPool::Task ta{[&]() { a++; }};

        auto tb = std::make_shared<ambthread::ThreadPool::Task>();
        tb->setFunction(
            [&]()
            {
                sleep();
                b++;
                pool.stop();
            });

        ambthread::ThreadPool::Task tc{[&]() { c++; }};
        ambthread::ThreadPool::Task td{[&]() { d++; }};

        tb->addDependency(std::move(ta));
        tc.addDependency(tb);
        td.addDependency(std::move(tc));
        pool.submit(std::move(td));
        pool.wait();

        REQUIRE(a == 1);
        REQUIRE(b == 1);
        REQUIRE(c == 0);
        REQUIRE(d == 0);
        REQUIRE(pool.empty());
        REQUIRE(!pool.stopping());

        int e = 0;
        pool.submit([&]() { e++; });
        pool.wait();
        REQUIRE(e == 1);
        REQUIRE(!pool.stopping());
    }
}
