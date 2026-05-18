require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "dependency")
require File.join(BUNDLER_RB_DIR, "bundler", "ruby_version")
require File.join(BUNDLER_RB_DIR, "bundler", "source")
require File.join(BUNDLER_RB_DIR, "bundler", "source_list")

module Bundler
  class DSL
    attr_reader :dependencies, :sources, :ruby_version

    def initialize
      @dependencies = []
      @sources = Bundler::SourceList.new
      @group_stack = []
      @source_stack = []
      @ruby_version = nil
      @gemfile_dir_stack = []
    end

    def self.evaluate(gemfile, _lockfile = nil, _unlock = nil)
      dsl = new
      dsl.eval_gemfile(gemfile)
      dsl
    end

    def source(url, &_block)
      rubygems = Bundler::Source::Rubygems.new(remote: url)
      @sources.add_source(rubygems)
      rubygems
    end

    def ruby(*requirements)
      @ruby_version = Bundler::RubyVersion.new(requirements)
    end

    def path(p, &blk)
      expanded = File.expand_path(p, _current_gemfile_dir)
      source = Bundler::Source::Path.new(path: expanded)
      @sources.add_source(source)
      @source_stack << source
      if blk
        instance_eval(&blk)
        @source_stack.pop
      end
      source
    end

    def git(uri, options = {}, &blk)
      source = Bundler::Source::Git.new(options.merge(git: uri))
      @sources.add_source(source)
      @source_stack << source
      if blk
        instance_eval(&blk)
        @source_stack.pop
      end
      source
    end

    def gem(name, *requirements)
      options = requirements.last.is_a?(Hash) ? requirements.pop : {}
      # Inline path/git options expand relative to current gemfile dir
      if options[:path]
        options[:path] = File.expand_path(options[:path], _current_gemfile_dir)
      end
      merged = options.dup
      merged[:groups] ||= current_groups
      merged[:source] ||= source_for_options(merged)
      @dependencies << Bundler::Dependency.new(name, requirements.empty? ? ">= 0" : requirements, merged)
    end

    def group(*groups, &blk)
      @group_stack << groups
      instance_eval(&blk) if blk
      @group_stack.pop
    end

    def eval_gemfile(path)
      full = File.expand_path(path, _current_gemfile_dir)
      dir  = File.dirname(full)
      content = File.read(full)
      @gemfile_dir_stack.push(dir)
      instance_eval(content, full, 1)
      self
    ensure
      @gemfile_dir_stack.pop
    end

    private

    def _current_gemfile_dir
      @gemfile_dir_stack.empty? ? Dir.pwd : @gemfile_dir_stack.last
    end

    def current_groups
      groups = []
      @group_stack.each { |entry| groups.concat(entry) }
      groups.empty? ? [:default] : groups
    end

    def source_for_options(options)
      return @source_stack[-1] if @source_stack.any?

      if options[:path]
        source = Bundler::Source::Path.new(path: options[:path])
        @sources.add_source(source)
        source
      elsif options[:git]
        source = Bundler::Source::Git.new(git: options[:git], branch: options[:branch], ref: options[:ref])
        @sources.add_source(source)
        source
      else
        @sources.default_source
      end
    end
  end
end
