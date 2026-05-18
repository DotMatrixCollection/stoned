require "rubygems"
require "rbconfig"

BUNDLER_RB_DIR = File.dirname(File.expand_path(__FILE__))

module Bundler
  VERSION = "2.5.0.stoned"

  class GemfileNotFound < RuntimeError; end
  class GemNotFound     < RuntimeError; end
  class BundlerError    < RuntimeError; end
  class LockfileError   < BundlerError; end
  class PermissionError < BundlerError; end

  class << self
    def root
      gemfile_name = ENV["BUNDLE_GEMFILE"]
      if gemfile_name && !gemfile_name.empty?
        return File.dirname(File.expand_path(gemfile_name))
      end

      dir = Dir.pwd
      loop do
        return dir if File.exist?(File.join(dir, "Gemfile"))
        parent = File.dirname(dir)
        break if parent == dir
        dir = parent
      end
      raise GemfileNotFound, "Could not locate Gemfile"
    end

    def gemfile
      gemfile_name = ENV["BUNDLE_GEMFILE"]
      if gemfile_name && !gemfile_name.empty?
        File.expand_path(gemfile_name)
      else
        File.join(root, "Gemfile")
      end
    end

    def lockfile
      File.join(root, "Gemfile.lock")
    end

    def setup(*groups)
      require File.join(BUNDLER_RB_DIR, "bundler", "setup")
      self
    end

    def require(*groups)
      # Set up load path first
      setup

      gemfile_path = gemfile
      lockfile_path = lockfile
      return unless File.exist?(gemfile_path)
      return unless File.exist?(lockfile_path)

      requested_groups = groups.flatten.map { |group| group.to_s }
      requested_groups = ["default"] if requested_groups.empty?

      parse_gemfile_dependencies(gemfile_path).each do |dep|
        dep_groups = dep[:groups] || ["default"]
        next if (dep_groups & requested_groups).empty?

        req_setting = dep[:require]
        next if req_setting == false
        if req_setting.is_a?(Array)
          req_setting.each { |r| require r rescue nil }
        elsif req_setting.is_a?(String)
          require req_setting rescue nil
        else
          require dep[:name] rescue nil
        end
      end

      self
    end

    def environment
      self
    end

    def settings
      @settings ||= {}
    end

    def bundle_path
      Gem.home
    end

    def with_unbundled_env(&block)
      block.call if block_given?
    end
    alias unbundled_env with_unbundled_env
    alias with_clean_env with_unbundled_env
    alias clean_env with_unbundled_env

    def ui
      @ui ||= BundlerUI.new
    end

    def load
      self
    end

    def locked_gems
      nil
    end

    private

    def parse_gemfile_dependencies(gemfile_path)
      deps = []
      group_stack = []

      begin
        File.readlines(gemfile_path).each do |line|
          stripped = line.strip
          next if stripped.empty? || stripped.start_with?("#")

          if stripped =~ /^group\s+(.+?)\s+do\s*$/
            groups = $1.scan(/:([A-Za-z0-9_]+)/).flatten
            group_stack << groups
            next
          end

          if stripped == "end"
            group_stack.pop if group_stack.any?
            next
          end

          next unless stripped =~ /^gem\s+['"]([^'"]+)['"](.*)$/

          gem_name = $1
          rest = $2
          dep_groups = []
          group_stack.each { |groups| dep_groups.concat(groups) }
          dep_groups.concat(extract_inline_groups(rest))
          dep_groups = ["default"] if dep_groups.empty?

          deps << {
            name: gem_name,
            groups: dep_groups.uniq,
            require: extract_require_setting(gem_name, rest),
          }
        end
      rescue Exception
        return []
      end

      deps
    end

    def extract_inline_groups(rest)
      groups = []

      if rest =~ /groups?:\s*\[([^\]]*)\]/
        groups.concat($1.scan(/:([A-Za-z0-9_]+)/).flatten)
      end

      if rest =~ /groups?:\s*:([A-Za-z0-9_]+)/
        groups << $1
      end

      groups
    end

    def extract_require_setting(gem_name, rest)
      return gem_name unless rest =~ /require:\s*(false|true|\[[^\]]*\]|"[^"]*"|'[^']*')/

      token = $1
      return false if token == "false"
      return gem_name if token == "true"
      return token.scan(/['"]([^'"]+)['"]/).flatten if token.start_with?("[")

      token[1...-1]
    end
  end

  class BundlerUI
    def info(msg, newline = true); end
    def warn(msg, newline = true); end
    def error(msg, newline = true); end
    def debug(msg, newline = true); end
    def confirm(msg, newline = true); end
    def silence; yield; end
  end
end
