require "bundler"

module Bundler
  class Index
    def initialize
      @specs = []
    end

    def <<(spec)
      @specs << spec
      self
    end

    def size
      @specs.length
    end

    def empty?
      @specs.empty?
    end

    def search(dependency)
      @specs.select do |spec|
        next false unless spec.name == dependency.name
        dependency.requirement.satisfied_by?(Gem::Version.new(spec.version))
      end
    end

    def [](name)
      @specs.select { |spec| spec.name == name.to_s }
    end

    def each(&block)
      @specs.each(&block)
    end
  end
end
