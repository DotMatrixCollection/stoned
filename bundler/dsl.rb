require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "dependency")

module Bundler
  class DSL
    attr_reader :dependencies, :sources

    def initialize
      @dependencies = []
      @sources = []
      @group_stack = []
    end

    def self.evaluate(gemfile, _lockfile = nil, _unlock = nil)
      dsl = new
      dsl.eval_gemfile(gemfile)
      dsl
    end

    def source(source)
      @sources << source.to_s
    end

    def gem(name, *requirements)
      options = requirements.last.is_a?(Hash) ? requirements.pop : {}
      merged = options.dup
      merged[:groups] ||= current_groups
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
          gem(name, options)
        elsif stripped =~ /^group\s+(.+?)\s+do\s*$/
          groups = $1.scan(/:([A-Za-z0-9_]+)/).flatten.map(&:to_sym)
          @group_stack << groups
        elsif stripped == "end"
          @group_stack.pop if @group_stack.any?
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
  end
end
