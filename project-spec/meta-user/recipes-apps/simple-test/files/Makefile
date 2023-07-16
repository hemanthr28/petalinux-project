APP = simple-test

# Add any other object files to this list below

APP_OBJS = simple-test.o
#CFLAGS= -O0

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS= O0)-o $@ $(APP_OBJS) $(LDLIBS)

clean:
	rm -f $(APP_OBJS) $(APP)