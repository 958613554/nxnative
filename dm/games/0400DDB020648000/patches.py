from nso_asm import B, CBNZW, NOP


def publish_shader_after_initialization(ctx):
    # REDengine takes a slot off the free list under its cache lock, then fills
    # the shader record outside the lock. Keep its refcount nonpositive until
    # that work is complete. Cache lookups already skip nonpositive refcounts
    # and acquire-load the count before reading the record.
    reserve = 0x84DD30
    finish_typed = 0x84DED8
    finish_regular = 0x84DFE4
    finish_compute = 0x84E154
    return_handle = 0x84E088
    for site, original in (
        (reserve, '6983e9b8'),
        (finish_typed, '8a0d0035'),
        (finish_regular, '29000014'),
        (finish_compute, 'cdffff17'),
    ):
        ctx.site(site, original)

    publish = ctx.here()
    ctx.emit(0x52800028)  # mov w8, #1
    ctx.emit(0x889FFF68)  # stlr w8, [x27], paired with the lookup's ldapr
    ctx.emit(B(ctx.here(), return_handle))

    typed = ctx.here()
    ctx.emit(CBNZW(10, typed, publish))  # loader modes with no more setup
    ctx.emit(B(ctx.here(), finish_typed + 4))  # mode zero still has metadata to fill

    # Cache hits and failures retain their original exits. Two misses for the
    # same key can reserve separate entries, as in the original miss path.
    ctx.put(reserve, [NOP], 'shader cache: keep reserved entry private')
    ctx.put(finish_typed, [B(finish_typed, typed)], 'shader cache: publish after fast load')
    ctx.put(finish_regular, [B(finish_regular, publish)], 'shader cache: publish after metadata load')
    ctx.put(finish_compute, [B(finish_compute, publish)], 'shader cache: publish after compute program initialization')


STUBS = {'afd670b6ba27b9e68d268e601bf91acd': publish_shader_after_initialization}

BINDING = [
    {'what': 'Publish shader-cache references after initialization',
     'now': 'build', 'goal': 'engine', 'purpose': 'race',
     'why': 'REDengine publishes a positive reference count while the shader record is still being filled. A second worker can acquire the half-initialized record.',
     'blocked': 'the Switch 2 executable offsets are verified only for v262144',
     'checked': 'v262144 Mac builds: engine record read before publication, original SDK shader-control access, and all loader return paths verified'},
]
