const std = @import("std");
const builtin = @import("builtin");

pub const Shared = struct {
    const Self = @This();

    const shm = struct {
        pub const open = std.c.shm_open;
        pub const unlink = std.c.shm_unlink;
    };

    pub const Data = struct {
        length: u32,
        numbers: [31]u32,
    };

    const Value = struct {
        busy: bool,
        data: Data,
    };

    name: [*:0]const u8,
    ptr: *Value,
    create: bool,

    pub fn init(_name: [*:0]const u8, comptime create: bool) !Self {
        _ = _name;
        const name = "/tmp/some_name8";
        var self: Self = .{
            .name = name,
            .ptr = undefined,
            .create = create,
        };
        const oflags: c_int = @bitCast(std.posix.O{
            .ACCMODE = .RDWR,
            .CREAT = true,
            .EXCL = false,
        });

        const mode = std.posix.S.IRWXU | std.posix.S.IRWXG | std.posix.S.IRWXO;
        try std.io.getStdOut().writer().print("Share mode: {o:0>3}\n", .{mode});
        const rc = std.c.shm_open(name, oflags, mode);
        try std.io.getStdOut().writer().print("Share fd: {}\n", .{rc});
        if (rc < 0) {
            return switch (std.posix.errno(rc)) {
                .ACCES => error.AccessDenied,
                .EXIST => error.ShareExists,
                .INVAL => error.InvalidName,
                .MFILE, .NFILE => error.TooManyFiles,
                .NAMETOOLONG => error.NameTooLong,
                .NOENT => error.ShareNotFound,
                .NOMEM => error.OutOfMemory,
                .NOSPC => error.NoSpaceLeft,
                .ROFS => error.ReadOnlyFileSystem,
                else => unreachable,
            };
        }

        if (create) {
            const truncate_rc = std.c.ftruncate(rc, @sizeOf(Value));
            if (truncate_rc < 0) {
                return switch (std.posix.errno(truncate_rc)) {
                    .ACCES => error.AccessDenied,
                    .BADF => error.BadFileDescriptor,
                    .INVAL => error.InvalidArgument,
                    .TXTBSY => error.FileBusy,
                    else => error.Unknown,
                };
            }
        }

        const rc_stat = try std.posix.fstat(
            rc,
        );

        try std.io.getStdOut().writer().print("Share permissions: {}\n", .{rc_stat.mode});

        const raw_data = try std.posix.mmap(
            null,
            @sizeOf(Value),
            std.posix.PROT.READ | std.posix.PROT.WRITE,
            std.c.MAP{ .TYPE = .SHARED },
            rc,
            0,
        );
        self.ptr = @ptrCast(raw_data);

        if (create) {
            self.ptr.busy = false;
            self.ptr.data.length = 0;
        }
        self.fill(&[_]u32{0} ** 10);

        return self;
    }

    pub fn deinit(self: *Self) void {
        const raw_data: [*]u8 = @ptrCast(self.ptr);
        std.posix.munmap(@alignCast(raw_data[0..@sizeOf(Value)]));
        if (self.create) {
            _ = shm.unlink(self.name);
        }
    }

    pub inline fn lock(self: *Self, comptime value: bool) void {
        while (true) {
            _ = @cmpxchgStrong(bool, &self.ptr.busy, !value, value, .seq_cst, .seq_cst) orelse break;
            std.Thread.yield() catch {};
        }
    }

    pub fn fill(self: *Self, data: []const u32) void {
        self.lock(true);
        defer self.lock(false);

        self.ptr.data.length = @intCast(data.len);
        for (data, 0..) |v, i| {
            self.ptr.data.numbers[i] = v;
        }
    }

    pub fn append(self: *Self, value: u32) void {
        self.lock(true);
        defer self.lock(false);

        if (self.ptr.data.length < self.ptr.data.numbers.len) {
            self.ptr.data.numbers[self.ptr.data.length] = value;
            self.ptr.data.length += 1;
        }
    }
};
