// ============================================================================
// HelloWorld.java -- Multithreaded Hello World example
// ============================================================================
//
// Copyright 2013 Tilera Corporation. All Rights Reserved.
//
//   The source code contained or described herein and all documents
//   related to the source code ("Material") are owned by Tilera
//   Corporation or its suppliers or licensors.  Title to the Material
//   remains with Tilera Corporation or its suppliers and licensors. The
//   software is licensed under the Tilera MDE License.
//
//   Unless otherwise agreed by Tilera in writing, you may not remove or
//   alter this notice or any other notice embedded in Materials by Tilera
//   or Tilera's suppliers or licensors in any way.
//
// ============================================================================

// package declaration
package com.tilera.examples;

// Java API classes
import java.util.List;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicInteger;

// custom classes

// ----------------------------------------------------------------------------
// HelloWorld
// ----------------------------------------------------------------------------

/** Multithreaded Hello World example. */
public class HelloWorld
    implements Runnable
{
    // --- main method ---

    /** Main method. */
    static public void main(String[] args)
    {
        HelloWorld hw = new HelloWorld();
        try {
            hw.run();
        }
        finally {
            hw.dispose();
        }
    }


    // --- members ---

    /** Number of threads to spawn. */
    protected int m_numThreads;

    /** List of thread instances. */
    protected List<ThreadInstance> m_threads;

    /** Number of threads reported in. */
    protected AtomicInteger m_threadCount;


    // --- constructors/destructors ---

    /** Constructor. */
    public HelloWorld() {
    	int cpus = getCPUCount();
    	println("Found %s cpus...", cpus);
        m_numThreads = cpus;
        m_threadCount = new AtomicInteger(0);
        m_threads = new ArrayList<ThreadInstance>();
    }

    /** Dispose method. */
    public void dispose() {
        if (m_threads != null) {
            for (ThreadInstance instance : m_threads) {
                instance.dispose();
            }
            m_threads.clear();
            m_threads = null;
        }
        if (m_threadCount != null) {
            m_threadCount = null;
        }
    }


    // --- methods ---

    /** Runs the example. */
    public void run() {
        println("Creating %s threads...", m_numThreads);
        for (int i=0; i<m_numThreads; ++i) {
            ThreadInstance instance = new ThreadInstance(i);
            m_threads.add(instance);
            instance.start();
        }

        println("Waiting for threads to report in...");
        while (getThreadCount() < m_numThreads) {
            try { Thread.sleep(500); }
            catch (InterruptedException e) {}
        }

        println("All threads active. Sleeping for a bit...");
        try { Thread.sleep(10000); }
        catch (InterruptedException e) {}

        // let threads know they can exit now
        println("Telling threads to shut down...");
        for (ThreadInstance instance : m_threads) {
            instance.interrupt();
        }

        println("Waiting for threads to shut down...");
        while (getThreadCount() > 0) {
            try { Thread.sleep(500); }
            catch (InterruptedException e) {}
        }

        println("Done.");
    }


    // --- thread-safe access methods ---

    /** Gets thread count. */
    protected int getThreadCount() {
        return m_threadCount.get();
    }

    /** Invoked by thread to increment thread count. */
    protected int incrementThreadCount() {
        return m_threadCount.incrementAndGet();
    }

    /** Invoked by thread to decrement thread count. */
    protected int decrementThreadCount() {
        return m_threadCount.decrementAndGet();
    }


    // --- utilities ---

    /** Gets number of CPUs/cores available. */
    static public int getCPUCount() {
        return Runtime.getRuntime().availableProcessors();
    }

    /** Prints formatted text to standard out. */
    static public void println(String format, Object... values)
    {
        System.out.format(format, values);
        System.out.println();
    }

    /** Prints formatted text to standard out. */
    static public void print(String format, Object... values)
    {
        System.out.format(format, values);
    }


    // ----------------------------------------------------------------------------
    // HelloWorld.ThreadInstance
    // ----------------------------------------------------------------------------

    // Note that this is an inner class, so it has implicit access
    // to the members/methods of the HelloWorld instance that created it.

    /** Thread instance class. */
    public class ThreadInstance extends Thread
    {
        // --- members ---

        /** ID of this instance. */
        protected int m_id;

        // --- constructors/destructors ---

        /** Constructor. */
        public ThreadInstance(int id) {
            m_id = id;
        }

        /** Dispose method. */
        public void dispose() {
            // nothing to do
        }

        // --- Thread implementation ---

        /** Invoked by thread when it is started. */
        public void run() {
            try {
                // indicate that we're ready to go
                incrementThreadCount();
                println("Hello, World from thread %s!", m_id);

                // can set a breakpoint here to stop in a single thread
                if (m_id == 3) {
                	println("A special hello from thread %s!", m_id);
                }

                // wait for parent to signal we can shut down
                while (! isInterrupted()) {

                    // do some busy work
                    loop(100);

                }
            }
            finally {
                println("Goodbye, World from thread %s!", m_id);
                decrementThreadCount();
            }
        }


        // --- Profilable methods ---

        /** Loops indicated number of times. */
        long loop(long loop) {
            long result = 0;
            while (--loop > 0) {
                a(loop);
            }
            return result;
        }

        /** Loops indicated number of times. */
        long a(long n) {
            return b(n);
        }

        /** Loops indicated number of times. */
        long b(long n) {
            return c(n);
        }

        /** Loops indicated number of times. */
        long c(long n) {
            return d(n);
        }

        /** Loops indicated number of times. */
        long d(long n) {
            return e(n);
        }

        /** Loops indicated number of times. */
        long e(long n) {
            long result = 1;
            for (long i=0; i<n; ++i) {
                result *= i;
            }
            return result;
        }
    }
}
