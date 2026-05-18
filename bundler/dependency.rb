require "bundler"

module Bundler
  class Dependency
    attr_reader :name, :requirement, :groups, :source, :autorequire

    def initialize(name, requirement = ">= 0", options = {})
      @name = name.to_s
      reqs = requirement.is_a?(Array) ? requirement : [requirement]
      @requirement = Gem::Requirement.new(*reqs)
      @groups = Array(options[:group] || options[:groups] || [:default]).map { |g| g.to_sym rescue g }
      @source = options[:source] || options[:path] || options[:git]
      @autorequire =
        if options[:require] == false
          []
        elsif options[:require].is_a?(Array)
          options[:require].map(&:to_s)
        elsif options.key?(:require)
          [options[:require].to_s]
        else
          [@name]
        end
    end

    def gem_platforms(_platforms = nil)
      self
    end

    def current_platform?
      true
    end

    def should_include?
      true
    end

    def to_lock
      @name
    end
  end
end
