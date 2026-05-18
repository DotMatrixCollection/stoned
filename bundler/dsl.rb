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
    end

    def self.evaluate(gemfile, _lockfile = nil, _unlock = nil)
      dsl = new
      dsl.eval_gemfile(gemfile)
      dsl
    end

    def source(source)
      rubygems = Bundler::Source::Rubygems.new(remote: source)
      @sources.add_source(rubygems)
      rubygems
    end

    def ruby(*requirements)
      @ruby_version = Bundler::RubyVersion.new(requirements)
    end

    def path(path)
      source = Bundler::Source::Path.new(path: path)
      @sources.add_source(source)
      @source_stack << source
      yield self if block_given?
      source
    ensure
      @source_stack.pop if block_given?
    end

    def git(uri, options = {})
      source = Bundler::Source::Git.new(options.merge(git: uri))
      @sources.add_source(source)
      @source_stack << source
      yield self if block_given?
      source
    ensure
      @source_stack.pop if block_given?
    end

    def gem(name, *requirements)
      options = requirements.last.is_a?(Hash) ? requirements.pop : {}
      merged = options.dup
      merged[:groups] ||= current_groups
      merged[:source] ||= source_for_options(merged)
      @dependencies << Bundler::Dependency.new(name, requirements.empty? ? ">= 0" : requirements, merged)
    end

    def group(*groups)
      @group_stack << groups
      yield self if block_given?
    ensure
      @group_stack.pop
    end

    def eval_gemfile(path)
      full = File.expand_path(path)
      root = File.dirname(full)
      File.readlines(full).each do |line|
        stripped = line.strip
        next if stripped.empty? || stripped.start_with?("#")

        if stripped =~ /^source\s+['"]([^'"]+)['"]/
          source($1)
        elsif stripped =~ /^ruby\s+['"]([^'"]+)['"]/
          ruby($1)
        elsif stripped =~ /^eval_gemfile\s+['"]([^'"]+)['"]/
          eval_gemfile(File.expand_path($1, root))
        elsif stripped =~ /^path\s+['"]([^'"]+)['"]\s+do\s*$/
          source = Bundler::Source::Path.new(path: File.expand_path($1, root))
          @sources.add_source(source)
          @source_stack << source
        elsif stripped =~ /^git\s+['"]([^'"]+)['"](?:\s*,\s*branch:\s*['"]([^'"]+)['"])?(?:\s*,\s*ref:\s*['"]([^'"]+)['"])?\s+do\s*$/
          source = Bundler::Source::Git.new(git: $1, branch: $2, ref: $3)
          @sources.add_source(source)
          @source_stack << source
        elsif stripped =~ /^gem\s+['"]([^'"]+)['"]/
          name = $1
          options = {}
          options[:require] = false if stripped.include?("require: false")
          if stripped =~ /require:\s*\[([^\]]*)\]/
            options[:require] = $1.scan(/['"]([^'"]+)['"]/).flatten
          elsif stripped =~ /require:\s*['"]([^'"]+)['"]/
            options[:require] = $1
          end
          if stripped =~ /path:\s*['"]([^'"]+)['"]/
            options[:path] = File.expand_path($1, root)
          end
          if stripped =~ /git:\s*['"]([^'"]+)['"]/
            options[:git] = $1
          end
          if stripped =~ /branch:\s*['"]([^'"]+)['"]/
            options[:branch] = $1
          end
          if stripped =~ /ref:\s*['"]([^'"]+)['"]/
            options[:ref] = $1
          end
          gem(name, options)
        elsif stripped =~ /^group\s+(.+?)\s+do\s*$/
          groups = $1.scan(/:([A-Za-z0-9_]+)/).flatten.map(&:to_sym)
          @group_stack << groups
        elsif stripped == "end"
          if @source_stack.any?
            @source_stack.pop
          elsif @group_stack.any?
            @group_stack.pop
          end
        end
      end
      self
    end

    private

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
