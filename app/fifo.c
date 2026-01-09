#include "app_config.h"
#include "fifo.h"

#include <hardware/sync.h>

static struct
{
	struct fifo_item fifo[KEY_FIFO_SIZE];
	volatile uint8_t count;
	volatile uint8_t read_idx;
	volatile uint8_t write_idx;
} self;

uint8_t fifo_count(void)
{
	// Single byte read is atomic, no critical section needed
	return self.count;
}

void fifo_flush(void)
{
	const uint32_t irq_state = save_and_disable_interrupts();

	self.write_idx = 0;
	self.read_idx = 0;
	self.count = 0;

	restore_interrupts(irq_state);
}

bool fifo_enqueue(const struct fifo_item item)
{
	const uint32_t irq_state = save_and_disable_interrupts();

	if (self.count >= KEY_FIFO_SIZE) {
		restore_interrupts(irq_state);
		return false;
	}

	self.fifo[self.write_idx++] = item;

	self.write_idx %= KEY_FIFO_SIZE;
	++self.count;

	restore_interrupts(irq_state);
	return true;
}

void fifo_enqueue_force(const struct fifo_item item)
{
	const uint32_t irq_state = save_and_disable_interrupts();

	if (self.count < KEY_FIFO_SIZE) {
		// Room available, normal enqueue
		self.fifo[self.write_idx++] = item;
		self.write_idx %= KEY_FIFO_SIZE;
		++self.count;
	} else {
		// FIFO full, overwrite oldest
		self.fifo[self.write_idx++] = item;
		self.write_idx %= KEY_FIFO_SIZE;

		self.read_idx++;
		self.read_idx %= KEY_FIFO_SIZE;
		// count stays the same (still full)
	}

	restore_interrupts(irq_state);
}

struct fifo_item fifo_dequeue(void)
{
	struct fifo_item item = { 0 };

	const uint32_t irq_state = save_and_disable_interrupts();

	if (self.count == 0) {
		restore_interrupts(irq_state);
		return item;
	}

	item = self.fifo[self.read_idx++];
	self.read_idx %= KEY_FIFO_SIZE;
	--self.count;

	restore_interrupts(irq_state);
	return item;
}
