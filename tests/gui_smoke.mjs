// Load from the Computer Use node_repl. Each call performs one UI action and
// refreshes state. Inspect its returned observation before choosing the next action.
export function guiDriver(sky, nodeRepl) {
  return {
    state: null, log: [], checks: 0,
    async bind(exe) {
      const windows = await sky.list_windows();
      const matches = windows.filter(w => w.app.toLowerCase() === ('process:' + exe).toLowerCase());
      if (matches.length !== 1) throw new Error(`Expected one test window; got ${matches.length}`);
      const w = matches[0];
      this.window = await sky.get_window({id:w.id, app:w.app});
      return this.observe(true);
    },
    async observe(screenshot=false) {
      this.state = await sky.get_window_state({window:this.window, include_screenshot:screenshot, include_text:true});
      this.window = this.state.window;
      nodeRepl.write(this.state.accessibility?.tree || 'No accessibility tree');
      return this.state;
    },
    async click(index, screenshot=false) {
      if (!this.state?.accessibility) throw new Error('Observe before acting');
      const w=this.state.window; this.state=null;
      await sky.click({window:w, element_index:index});
      this.log.push({action:'click', index, time:new Date().toISOString()});
      return this.observe(screenshot);
    },
    async set(index,value,screenshot=false) {
      if (!this.state?.accessibility) throw new Error('Observe before acting');
      const w=this.state.window; this.state=null;
      await sky.set_value({window:w, element_index:index, value});
      this.log.push({action:'set', index, time:new Date().toISOString()});
      return this.observe(screenshot);
    },
    async key(key,screenshot=false) {
      if (!this.state) throw new Error('Observe before acting');
      const w=this.state.window; this.state=null;
      await sky.press_key({window:w,key});
      this.log.push({action:'key',key,time:new Date().toISOString()});
      return this.observe(screenshot);
    },
    expect(text) {
      this.checks++;
      if (!this.state?.accessibility?.tree?.includes(text)) throw new Error(`GUI assertion failed: ${text}`);
      this.log.push({assertion:text,passed:true});
      nodeRepl.write(`PASS GUI ${this.checks}: ${text}`);
    }
  };
}
