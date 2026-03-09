package com.tactorder.gateway.application

import kotlinx.coroutines.CancellableContinuation
import kotlinx.coroutines.suspendCancellableCoroutine
import org.slf4j.LoggerFactory
import java.util.PriorityQueue
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

enum class Priority {
    CRITICAL, NORMAL, BACKGROUND
}

data class QueueConfig(
    val criticalSlaMs: Long = 1000,
    val normalSlaMs: Long = 10000,
    val backgroundSlaMs: Long = 300000,
    val maxQueueSize: Int = 1000
)

class QueueManager(private val config: QueueConfig) {
    private val logger = LoggerFactory.getLogger(QueueManager::class.java)

    private data class QueuedTask(
        val priority: Priority,
        val deadline: Long,
        val enqueueTime: Long,
        val continuation: CancellableContinuation<Unit>
    ) : Comparable<QueuedTask> {
        override fun compareTo(other: QueuedTask): Int {
            // Earliest Deadline First (EDF)
            val deadlineCmp = this.deadline.compareTo(other.deadline)
            if (deadlineCmp != 0) return deadlineCmp
            // Tie-breaker: Priority ordinal (CRITICAL < NORMAL < BACKGROUND)
            val priorityCmp = this.priority.compareTo(other.priority)
            if (priorityCmp != 0) return priorityCmp
            // Tie-breaker: FIFO
            return this.enqueueTime.compareTo(other.enqueueTime)
        }
    }

    private val queue = PriorityQueue<QueuedTask>()
    private val lock = Any()
    private var isExecuting = false

    suspend fun <T> execute(priority: Priority, block: suspend () -> T): T {
        acquire(priority)
        try {
            return block()
        } finally {
            release()
        }
    }

    private suspend fun acquire(priority: Priority) {
        val now = System.currentTimeMillis()
        val sla = when (priority) {
            Priority.CRITICAL -> config.criticalSlaMs
            Priority.BACKGROUND -> config.backgroundSlaMs
            Priority.NORMAL -> config.normalSlaMs
        }
        val deadline = now + sla

        var acquiredImmediately = false
        synchronized(lock) {
            if (!isExecuting) {
                isExecuting = true
                acquiredImmediately = true
            }
        }
        if (acquiredImmediately) return

        suspendCancellableCoroutine<Unit> { cont ->
            val task = QueuedTask(priority, deadline, now, cont)
            synchronized(lock) {
                if (!isExecuting) {
                    isExecuting = true
                    cont.resume(Unit)
                } else {
                    if (queue.size >= config.maxQueueSize) {
                        logger.warn("Queue full! Rejecting $priority task.")
                        cont.resumeWithException(IllegalStateException("Queue is full (size=${queue.size})"))
                    } else {
                        queue.add(task)
                        logger.debug("Enqueued $priority task. Queue size: ${queue.size}")
                        cont.invokeOnCancellation {
                            synchronized(lock) {
                                queue.remove(task)
                            }
                        }
                    }
                }
            }
        }
    }

    private fun release() {
        var nextTask: QueuedTask? = null
        synchronized(lock) {
            if (queue.isNotEmpty()) {
                nextTask = queue.poll()
                logger.debug("Dequeueing task with priority ${nextTask?.priority}. Queue size remaining: ${queue.size}")
            } else {
                isExecuting = false
            }
        }
        nextTask?.continuation?.resume(Unit)
    }
}
