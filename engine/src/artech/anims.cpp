#include "artech/anims.h"

#include <cstdio>
#include <iterator>

#include "artech/scripts.h"
#include "artech/context.h"

namespace edison {
namespace {

int16_t s16(const std::vector<uint8_t>& d, size_t at) { return static_cast<int16_t>(d[at] | d[at + 1] << 8); }

}  // namespace

const Anims::Def* Anims::load(uint16_t anim) {
    if (auto it = defs_.find(anim); it != defs_.end()) return &it->second;
    std::vector<uint8_t> d;
    if (!ctx_.read(anim, d) || d.size() < 8) return nullptr;
    Def def;
    def.id = anim;
    def.x = s16(d, 2);
    def.y = s16(d, 4);
    const int count = s16(d, 0);
    for (int i = 0; i < count && 8 + 7 * (i + 1) <= static_cast<int>(d.size()); ++i) {
        const size_t at = 8 + 7 * static_cast<size_t>(i);
        def.frames.push_back({s16(d, at), s16(d, at + 2), static_cast<uint16_t>(s16(d, at + 4)), d[at + 6]});
    }
    if (def.frames.empty()) return nullptr;
    return &defs_.emplace(anim, std::move(def)).first->second;
}

void Anims::start(uint16_t anim, int x, int y, ScriptContext* waiter, uint8_t delay, uint8_t layer,
                  bool loop, uint16_t sound) {
    const Def* def = load(anim);
    if (!def) {
        char buf[48];
        std::snprintf(buf, sizeof buf, "anim %04x missing", anim);
        warnOnce(buf);
        if (waiter) waiter->flags &= ~ScriptContext::kWaiting;
        return;
    }
    Instance a;
    a.def = def;
    a.waiter = waiter;
    a.delay = delay;
    a.layer = layer;
    a.flags = static_cast<uint8_t>((loop ? kLoop : 0) | kFirst);
    a.sound = sound;
    // Only x is tested for "not given", as in the original.
    a.x = x == -1 ? def->x : x;
    a.y = x == -1 ? def->y : y;

    // Insertion as in the original, including its quirk: a new anim with a
    // lower layer than the head only goes first when the list has one entry.
    if (list_.empty()) {
        list_.push_back(a);
    } else if (list_.size() == 1 && layer < list_.front().layer) {
        list_.push_front(a);
    } else {
        auto p = list_.begin();
        while (std::next(p) != list_.end() && std::next(p)->layer <= layer) ++p;
        list_.insert(std::next(p), a);
    }
    if (list_.size() == 1) nextTick_ = ctx_.gameTicks;  // f08_09ae
}

void Anims::stop(uint16_t anim) {
    for (auto& a : list_)
        if (a.def->id == anim) a.flags |= kStop;
}

void Anims::stopAll() {
    for (auto& a : list_) a.flags |= kStop;
}

void Anims::stopWaitedOnBy(const ScriptContext* script) {
    for (auto& a : list_)
        if (a.waiter == script) {
            a.waiter = nullptr;
            a.flags |= kStop;
        }
}

void Anims::clear() {
    list_.clear();
    dirty_.clear();
}

void Anims::step(Instance& a) {
    if (a.flags & kDead) return;
    if (a.delay) {
        --a.delay;
        return;
    }
    const auto& frames = a.def->frames;
    const int last = static_cast<int>(frames.size()) - 1;
    const Frame& f = frames[a.frame];

    // WAIT: the script carries on just before the last frame ends.
    if (a.waiter && !(a.flags & kLoop)) {
        bool release = false;
        if (a.frame == last - 1 && f.ticks <= a.counter && frames[a.frame + 1].ticks == 1) release = true;
        if (a.frame == last && f.ticks - 1 <= a.counter) release = true;
        if (release) {
            a.waiter->flags &= ~ScriptContext::kWaiting;
            a.waiter = nullptr;
        }
    }
    const bool stopping = (a.flags & (kStop | kDead)) != 0;
    if (stopping) a.counter = f.ticks;
    if (!stopping && a.counter < f.ticks) {
        ++a.counter;
        if (!(a.flags & kFirst)) return;
    }

    bool finished = false;
    if (a.frame == last && (a.flags & kLoop) && f.ticks <= a.counter) a.frame = -1;
    if (!stopping) {
        if (a.frame == last && a.counter == f.ticks) {
            finished = true;
        } else if (!(a.flags & kFirst)) {
            ++a.frame;
        }
        if (a.frame > last) finished = true;  // (zero-tick last frame; not in the data)
    } else {
        finished = true;
    }
    if (!(a.flags & kFirst)) dirty_.push_back(a.rect);
    if (finished) {
        a.bitmap = 0;
        a.flags |= kDead;
        ctx_.markFinished(a.def->id);
        if (a.waiter) {
            a.waiter->flags &= ~ScriptContext::kWaiting;
            a.waiter = nullptr;
        }
    } else {
        const Frame& shown = frames[a.frame];
        const Bitmap& bmp = ctx_.bitmap(shown.bitmap);
        a.bitmap = shown.bitmap;
        a.rect.left = a.x + shown.dx;
        a.rect.top = a.y + shown.dy;
        a.rect.right = a.rect.left + bmp.width - 1;
        a.rect.bottom = a.rect.top + bmp.height - 1;
        dirty_.push_back(a.rect);
    }
    a.counter = 1;
    a.flags &= ~kFirst;
    if (a.sound) {
        ctx_.playWav(a.sound);
        a.sound = 0;
    }
}

void Anims::redraw(const Rect& r) {
    // f08_0886. The copies use right - left and bottom - top as the size,
    // so the last column and row of a rectangle aren't refreshed.
    const int w = r.right - r.left, h = r.bottom - r.top;
    ctx_.screens.copyArea(3, 2, r.left, r.top, w, h);
    for (const auto& a : list_) {
        if (a.bitmap && a.rect.overlaps(r))
            ctx_.screens.drawSprite(2, ctx_.bitmap(a.bitmap), a.rect.left, a.rect.top);
    }
    ctx_.screens.copyArea(2, 1, r.left, r.top, w, h);
}

void Anims::tick() {
    if (ctx_.gameTicks < nextTick_) return;
    nextTick_ = ctx_.gameTicks + 1;
    for (auto& a : list_) step(a);
    list_.remove_if([](const Instance& a) { return (a.flags & kDead) != 0; });

    // Drop rectangles inside another (f08_09d4).
    std::vector<bool> dropped(dirty_.size());
    for (size_t i = 0; i < dirty_.size(); ++i) {
        if (dropped[i]) continue;
        for (size_t j = i + 1; j < dirty_.size(); ++j) {
            if (dropped[j]) continue;
            if (dirty_[i].contains(dirty_[j])) {
                dropped[j] = true;
            } else if (dirty_[j].contains(dirty_[i])) {
                dropped[i] = true;
                break;
            }
        }
    }
    for (size_t i = 0; i < dirty_.size(); ++i)
        if (!dropped[i]) redraw(dirty_[i]);
    dirty_.clear();
}

}  // namespace edison
